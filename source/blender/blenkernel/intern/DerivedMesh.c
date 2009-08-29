/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_particle_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_particle.h"
#include "BKE_bvhutils.h"

#include "BLO_sys_types.h" // for intptr_t support

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_material.h"

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
			dm->getNumFaces(dm));
		CustomData_set_layer_flag(&dm->faceData, CD_MFACE, CD_FLAG_TEMPORARY);
		dm->copyFaceArray(dm, mface);
	}

	return mface;
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
	MFace *tmp = MEM_callocN(sizeof(*tmp) * dm->getNumFaces(dm),
	                         "dm_dupFaceArray tmp");

	if(tmp) dm->copyFaceArray(dm, tmp);

	return tmp;
}

void DM_init_funcs(DerivedMesh *dm)
{
	/* default function implementations */
	dm->getVertArray = dm_getVertArray;
	dm->getEdgeArray = dm_getEdgeArray;
	dm->getFaceArray = dm_getFaceArray;
	dm->dupVertArray = dm_dupVertArray;
	dm->dupEdgeArray = dm_dupEdgeArray;
	dm->dupFaceArray = dm_dupFaceArray;

	dm->getVertData = DM_get_vert_data;
	dm->getEdgeData = DM_get_edge_data;
	dm->getFaceData = DM_get_face_data;
	dm->getVertDataArray = DM_get_vert_data_layer;
	dm->getEdgeDataArray = DM_get_edge_data_layer;
	dm->getFaceDataArray = DM_get_face_data_layer;

	bvhcache_init(&dm->bvhCache);
}

void DM_init(DerivedMesh *dm,
             int numVerts, int numEdges, int numFaces)
{
	dm->numVertData = numVerts;
	dm->numEdgeData = numEdges;
	dm->numFaceData = numFaces;

	DM_init_funcs(dm);
	
	dm->needsFree = 1;
}

void DM_from_template(DerivedMesh *dm, DerivedMesh *source,
                      int numVerts, int numEdges, int numFaces)
{
	CustomData_copy(&source->vertData, &dm->vertData, CD_MASK_DERIVEDMESH,
	                CD_CALLOC, numVerts);
	CustomData_copy(&source->edgeData, &dm->edgeData, CD_MASK_DERIVEDMESH,
	                CD_CALLOC, numEdges);
	CustomData_copy(&source->faceData, &dm->faceData, CD_MASK_DERIVEDMESH,
	                CD_CALLOC, numFaces);

	dm->numVertData = numVerts;
	dm->numEdgeData = numEdges;
	dm->numFaceData = numFaces;

	DM_init_funcs(dm);

	dm->needsFree = 1;
}

int DM_release(DerivedMesh *dm)
{
	if (dm->needsFree) {
		bvhcache_free(&dm->bvhCache);

		CustomData_free(&dm->vertData, dm->numVertData);
		CustomData_free(&dm->edgeData, dm->numEdgeData);
		CustomData_free(&dm->faceData, dm->numFaceData);

		return 1;
	}
	else {
		CustomData_free_temporary(&dm->vertData, dm->numVertData);
		CustomData_free_temporary(&dm->edgeData, dm->numEdgeData);
		CustomData_free_temporary(&dm->faceData, dm->numFaceData);

		return 0;
	}
}

void DM_to_mesh(DerivedMesh *dm, Mesh *me)
{
	/* dm might depend on me, so we need to do everything with a local copy */
	Mesh tmp = *me;
	int totvert, totedge, totface;

	memset(&tmp.vdata, 0, sizeof(tmp.vdata));
	memset(&tmp.edata, 0, sizeof(tmp.edata));
	memset(&tmp.fdata, 0, sizeof(tmp.fdata));

	totvert = tmp.totvert = dm->getNumVerts(dm);
	totedge = tmp.totedge = dm->getNumEdges(dm);
	totface = tmp.totface = dm->getNumFaces(dm);

	CustomData_copy(&dm->vertData, &tmp.vdata, CD_MASK_MESH, CD_DUPLICATE, totvert);
	CustomData_copy(&dm->edgeData, &tmp.edata, CD_MASK_MESH, CD_DUPLICATE, totedge);
	CustomData_copy(&dm->faceData, &tmp.fdata, CD_MASK_MESH, CD_DUPLICATE, totface);

	/* not all DerivedMeshes store their verts/edges/faces in CustomData, so
	   we set them here in case they are missing */
	if(!CustomData_has_layer(&tmp.vdata, CD_MVERT))
		CustomData_add_layer(&tmp.vdata, CD_MVERT, CD_ASSIGN, dm->dupVertArray(dm), totvert);
	if(!CustomData_has_layer(&tmp.edata, CD_MEDGE))
		CustomData_add_layer(&tmp.edata, CD_MEDGE, CD_ASSIGN, dm->dupEdgeArray(dm), totedge);
	if(!CustomData_has_layer(&tmp.fdata, CD_MFACE))
		CustomData_add_layer(&tmp.fdata, CD_MFACE, CD_ASSIGN, dm->dupFaceArray(dm), totface);

	mesh_update_customdata_pointers(&tmp);

	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->fdata, me->totface);

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

///

static DerivedMesh *getMeshDerivedMesh(Mesh *me, Object *ob, float (*vertCos)[3])
{
	DerivedMesh *dm = CDDM_from_mesh(me, ob);
	
	if(!dm)
		return NULL;
	
	if (vertCos)
		CDDM_apply_vert_coords(dm, vertCos);

	CDDM_calc_normals(dm);

	return dm;
}

///

typedef struct {
	DerivedMesh dm;

	EditMesh *em;
	float (*vertexCos)[3];
	float (*vertexNos)[3];
	float (*faceNos)[3];
} EditMeshDerivedMesh;

static void emDM_foreachMappedVert(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no_f, short *no_s), void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditVert *eve;
	int i;

	for (i=0,eve= emdm->em->verts.first; eve; i++,eve=eve->next) {
		if (emdm->vertexCos) {
			func(userData, i, emdm->vertexCos[i], emdm->vertexNos[i], NULL);
		} else {
			func(userData, i, eve->co, eve->no, NULL);
		}
	}
}
static void emDM_foreachMappedEdge(DerivedMesh *dm, void (*func)(void *userData, int index, float *v0co, float *v1co), void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditEdge *eed;
	int i;

	if (emdm->vertexCos) {
		EditVert *eve;

		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;
		for(i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next)
			func(userData, i, emdm->vertexCos[(int) eed->v1->tmp.l], emdm->vertexCos[(int) eed->v2->tmp.l]);
	} else {
		for(i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next)
			func(userData, i, eed->v1->co, eed->v2->co);
	}
}
static void emDM_drawMappedEdges(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData) 
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditEdge *eed;
	int i;

	if (emdm->vertexCos) {
		EditVert *eve;

		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;

		glBegin(GL_LINES);
		for(i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next) {
			if(!setDrawOptions || setDrawOptions(userData, i)) {
				glVertex3fv(emdm->vertexCos[(int) eed->v1->tmp.l]);
				glVertex3fv(emdm->vertexCos[(int) eed->v2->tmp.l]);
			}
		}
		glEnd();
	} else {
		glBegin(GL_LINES);
		for(i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next) {
			if(!setDrawOptions || setDrawOptions(userData, i)) {
				glVertex3fv(eed->v1->co);
				glVertex3fv(eed->v2->co);
			}
		}
		glEnd();
	}
}
static void emDM_drawEdges(DerivedMesh *dm, int drawLooseEdges)
{
	emDM_drawMappedEdges(dm, NULL, NULL);
}
static void emDM_drawMappedEdgesInterp(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void (*setDrawInterpOptions)(void *userData, int index, float t), void *userData) 
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditEdge *eed;
	int i;

	if (emdm->vertexCos) {
		EditVert *eve;

		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;

		glBegin(GL_LINES);
		for (i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next) {
			if(!setDrawOptions || setDrawOptions(userData, i)) {
				setDrawInterpOptions(userData, i, 0.0);
				glVertex3fv(emdm->vertexCos[(int) eed->v1->tmp.l]);
				setDrawInterpOptions(userData, i, 1.0);
				glVertex3fv(emdm->vertexCos[(int) eed->v2->tmp.l]);
			}
		}
		glEnd();
	} else {
		glBegin(GL_LINES);
		for (i=0,eed= emdm->em->edges.first; eed; i++,eed= eed->next) {
			if(!setDrawOptions || setDrawOptions(userData, i)) {
				setDrawInterpOptions(userData, i, 0.0);
				glVertex3fv(eed->v1->co);
				setDrawInterpOptions(userData, i, 1.0);
				glVertex3fv(eed->v2->co);
			}
		}
		glEnd();
	}
}

static void emDM_drawUVEdges(DerivedMesh *dm)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditFace *efa;
	MTFace *tf;

	glBegin(GL_LINES);
	for(efa= emdm->em->faces.first; efa; efa= efa->next) {
		tf = CustomData_em_get(&emdm->em->fdata, efa->data, CD_MTFACE);

		if(tf && !(efa->h)) {
			glVertex2fv(tf->uv[0]);
			glVertex2fv(tf->uv[1]);

			glVertex2fv(tf->uv[1]);
			glVertex2fv(tf->uv[2]);

			if (!efa->v4) {
				glVertex2fv(tf->uv[2]);
				glVertex2fv(tf->uv[0]);
			} else {
				glVertex2fv(tf->uv[2]);
				glVertex2fv(tf->uv[3]);
				glVertex2fv(tf->uv[3]);
				glVertex2fv(tf->uv[0]);
			}
		}
	}
	glEnd();
}

static void emDM__calcFaceCent(EditFace *efa, float cent[3], float (*vertexCos)[3])
{
	if (vertexCos) {
		VECCOPY(cent, vertexCos[(int) efa->v1->tmp.l]);
		VecAddf(cent, cent, vertexCos[(int) efa->v2->tmp.l]);
		VecAddf(cent, cent, vertexCos[(int) efa->v3->tmp.l]);
		if (efa->v4) VecAddf(cent, cent, vertexCos[(int) efa->v4->tmp.l]);
	} else {
		VECCOPY(cent, efa->v1->co);
		VecAddf(cent, cent, efa->v2->co);
		VecAddf(cent, cent, efa->v3->co);
		if (efa->v4) VecAddf(cent, cent, efa->v4->co);
	}

	if (efa->v4) {
		VecMulf(cent, 0.25f);
	} else {
		VecMulf(cent, 0.33333333333f);
	}
}
static void emDM_foreachMappedFaceCenter(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no), void *userData)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditVert *eve;
	EditFace *efa;
	float cent[3];
	int i;

	if (emdm->vertexCos) {
		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;
	}

	for(i=0,efa= emdm->em->faces.first; efa; i++,efa= efa->next) {
		emDM__calcFaceCent(efa, cent, emdm->vertexCos);
		func(userData, i, cent, emdm->vertexCos?emdm->faceNos[i]:efa->n);
	}
}
static void emDM_drawMappedFaces(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index, int *drawSmooth_r), void *userData, int useColors)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditFace *efa;
	int i, draw;

	if (emdm->vertexCos) {
		EditVert *eve;

		for (i=0,eve=emdm->em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;

		for (i=0,efa= emdm->em->faces.first; efa; i++,efa= efa->next) {
			int drawSmooth = (efa->flag & ME_SMOOTH);
			draw = setDrawOptions==NULL ? 1 : setDrawOptions(userData, i, &drawSmooth);
			if(draw) {
				if (draw==2) { /* enabled with stipple */
		  			glEnable(GL_POLYGON_STIPPLE);
		  			glPolygonStipple(stipple_quarttone);
				}
				
				glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);

				glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
				if (!drawSmooth) {
					glNormal3fv(emdm->faceNos[i]);
					glVertex3fv(emdm->vertexCos[(int) efa->v1->tmp.l]);
					glVertex3fv(emdm->vertexCos[(int) efa->v2->tmp.l]);
					glVertex3fv(emdm->vertexCos[(int) efa->v3->tmp.l]);
					if(efa->v4) glVertex3fv(emdm->vertexCos[(int) efa->v4->tmp.l]);
				} else {
					glNormal3fv(emdm->vertexNos[(int) efa->v1->tmp.l]);
					glVertex3fv(emdm->vertexCos[(int) efa->v1->tmp.l]);
					glNormal3fv(emdm->vertexNos[(int) efa->v2->tmp.l]);
					glVertex3fv(emdm->vertexCos[(int) efa->v2->tmp.l]);
					glNormal3fv(emdm->vertexNos[(int) efa->v3->tmp.l]);
					glVertex3fv(emdm->vertexCos[(int) efa->v3->tmp.l]);
					if(efa->v4) {
						glNormal3fv(emdm->vertexNos[(int) efa->v4->tmp.l]);
						glVertex3fv(emdm->vertexCos[(int) efa->v4->tmp.l]);
					}
				}
				glEnd();
				
				if (draw==2)
					glDisable(GL_POLYGON_STIPPLE);
			}
		}
	} else {
		for (i=0,efa= emdm->em->faces.first; efa; i++,efa= efa->next) {
			int drawSmooth = (efa->flag & ME_SMOOTH);
			draw = setDrawOptions==NULL ? 1 : setDrawOptions(userData, i, &drawSmooth);
			if(draw) {
				if (draw==2) { /* enabled with stipple */
		  			glEnable(GL_POLYGON_STIPPLE);
		  			glPolygonStipple(stipple_quarttone);
				}
				glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);

				glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
				if (!drawSmooth) {
					glNormal3fv(efa->n);
					glVertex3fv(efa->v1->co);
					glVertex3fv(efa->v2->co);
					glVertex3fv(efa->v3->co);
					if(efa->v4) glVertex3fv(efa->v4->co);
				} else {
					glNormal3fv(efa->v1->no);
					glVertex3fv(efa->v1->co);
					glNormal3fv(efa->v2->no);
					glVertex3fv(efa->v2->co);
					glNormal3fv(efa->v3->no);
					glVertex3fv(efa->v3->co);
					if(efa->v4) {
						glNormal3fv(efa->v4->no);
						glVertex3fv(efa->v4->co);
					}
				}
				glEnd();
				
				if (draw==2)
					glDisable(GL_POLYGON_STIPPLE);
			}
		}
	}
}

static void emDM_drawFacesTex_common(DerivedMesh *dm,
               int (*drawParams)(MTFace *tface, MCol *mcol, int matnr),
               int (*drawParamsMapped)(void *userData, int index),
               void *userData) 
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditMesh *em= emdm->em;
	float (*vertexCos)[3]= emdm->vertexCos;
	float (*vertexNos)[3]= emdm->vertexNos;
	EditFace *efa;
	int i;

	/* always use smooth shading even for flat faces, else vertex colors wont interpolate */
	glShadeModel(GL_SMOOTH);
	
	if (vertexCos) {
		EditVert *eve;

		for (i=0,eve=em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;

		for (i=0,efa= em->faces.first; efa; i++,efa= efa->next) {
			MTFace *tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			MCol *mcol= CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
			unsigned char *cp= NULL;
			int drawSmooth= (efa->flag & ME_SMOOTH);
			int flag;

			if(drawParams)
				flag= drawParams(tf, mcol, efa->mat_nr);
			else if(drawParamsMapped)
				flag= drawParamsMapped(userData, i);
			else
				flag= 1;

			if(flag != 0) { /* flag 0 == the face is hidden or invisible */
				
				/* we always want smooth here since otherwise vertex colors dont interpolate */
				if (mcol) {
					if (flag==1) {
						cp= (unsigned char*)mcol;
					}
				} else {
					glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);
				} 
				
				glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
				if (!drawSmooth) {
					glNormal3fv(emdm->faceNos[i]);

					if(tf) glTexCoord2fv(tf->uv[0]);
					if(cp) glColor3ub(cp[3], cp[2], cp[1]);
					glVertex3fv(vertexCos[(int) efa->v1->tmp.l]);

					if(tf) glTexCoord2fv(tf->uv[1]);
					if(cp) glColor3ub(cp[7], cp[6], cp[5]);
					glVertex3fv(vertexCos[(int) efa->v2->tmp.l]);

					if(tf) glTexCoord2fv(tf->uv[2]);
					if(cp) glColor3ub(cp[11], cp[10], cp[9]);
					glVertex3fv(vertexCos[(int) efa->v3->tmp.l]);

					if(efa->v4) {
						if(tf) glTexCoord2fv(tf->uv[3]);
						if(cp) glColor3ub(cp[15], cp[14], cp[13]);
						glVertex3fv(vertexCos[(int) efa->v4->tmp.l]);
					}
				} else {
					if(tf) glTexCoord2fv(tf->uv[0]);
					if(cp) glColor3ub(cp[3], cp[2], cp[1]);
					glNormal3fv(vertexNos[(int) efa->v1->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v1->tmp.l]);

					if(tf) glTexCoord2fv(tf->uv[1]);
					if(cp) glColor3ub(cp[7], cp[6], cp[5]);
					glNormal3fv(vertexNos[(int) efa->v2->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v2->tmp.l]);

					if(tf) glTexCoord2fv(tf->uv[2]);
					if(cp) glColor3ub(cp[11], cp[10], cp[9]);
					glNormal3fv(vertexNos[(int) efa->v3->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v3->tmp.l]);

					if(efa->v4) {
						if(tf) glTexCoord2fv(tf->uv[3]);
						if(cp) glColor3ub(cp[15], cp[14], cp[13]);
						glNormal3fv(vertexNos[(int) efa->v4->tmp.l]);
						glVertex3fv(vertexCos[(int) efa->v4->tmp.l]);
					}
				}
				glEnd();
			}
		}
	} else {
		for (i=0,efa= em->faces.first; efa; i++,efa= efa->next) {
			MTFace *tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			MCol *mcol= CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
			unsigned char *cp= NULL;
			int drawSmooth= (efa->flag & ME_SMOOTH);
			int flag;

			if(drawParams)
				flag= drawParams(tf, mcol, efa->mat_nr);
			else if(drawParamsMapped)
				flag= drawParamsMapped(userData, i);
			else
				flag= 1;

			if(flag != 0) { /* flag 0 == the face is hidden or invisible */
				/* we always want smooth here since otherwise vertex colors dont interpolate */
				if (mcol) {
					if (flag==1) {
						cp= (unsigned char*)mcol;
					}
				} else {
					glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);
				} 

				glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
				if (!drawSmooth) {
					glNormal3fv(efa->n);

					if(tf) glTexCoord2fv(tf->uv[0]);
					if(cp) glColor3ub(cp[3], cp[2], cp[1]);
					glVertex3fv(efa->v1->co);

					if(tf) glTexCoord2fv(tf->uv[1]);
					if(cp) glColor3ub(cp[7], cp[6], cp[5]);
					glVertex3fv(efa->v2->co);

					if(tf) glTexCoord2fv(tf->uv[2]);
					if(cp) glColor3ub(cp[11], cp[10], cp[9]);
					glVertex3fv(efa->v3->co);

					if(efa->v4) {
						if(tf) glTexCoord2fv(tf->uv[3]);
						if(cp) glColor3ub(cp[15], cp[14], cp[13]);
						glVertex3fv(efa->v4->co);
					}
				} else {
					if(tf) glTexCoord2fv(tf->uv[0]);
					if(cp) glColor3ub(cp[3], cp[2], cp[1]);
					glNormal3fv(efa->v1->no);
					glVertex3fv(efa->v1->co);

					if(tf) glTexCoord2fv(tf->uv[1]);
					if(cp) glColor3ub(cp[7], cp[6], cp[5]);
					glNormal3fv(efa->v2->no);
					glVertex3fv(efa->v2->co);

					if(tf) glTexCoord2fv(tf->uv[2]);
					if(cp) glColor3ub(cp[11], cp[10], cp[9]);
					glNormal3fv(efa->v3->no);
					glVertex3fv(efa->v3->co);

					if(efa->v4) {
						if(tf) glTexCoord2fv(tf->uv[3]);
						if(cp) glColor3ub(cp[15], cp[14], cp[13]);
						glNormal3fv(efa->v4->no);
						glVertex3fv(efa->v4->co);
					}
				}
				glEnd();
			}
		}
	}
}

static void emDM_drawFacesTex(DerivedMesh *dm, int (*setDrawOptions)(MTFace *tface, MCol *mcol, int matnr))
{
	emDM_drawFacesTex_common(dm, setDrawOptions, NULL, NULL);
}

static void emDM_drawMappedFacesTex(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData)
{
	emDM_drawFacesTex_common(dm, NULL, setDrawOptions, userData);
}

static void emDM_drawMappedFacesGLSL(DerivedMesh *dm,
               int (*setMaterial)(int, void *attribs),
               int (*setDrawOptions)(void *userData, int index), void *userData) 
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditMesh *em= emdm->em;
	float (*vertexCos)[3]= emdm->vertexCos;
	float (*vertexNos)[3]= emdm->vertexNos;
	EditVert *eve;
	EditFace *efa;
	DMVertexAttribs attribs;
	GPUVertexAttribs gattribs;
	MTFace *tf;
	int transp, new_transp, orig_transp, tfoffset;
	int i, b, matnr, new_matnr, dodraw, layer;

	dodraw = 0;
	matnr = -1;

	transp = GPU_get_material_blend_mode();
	orig_transp = transp;
	layer = CustomData_get_layer_index(&em->fdata, CD_MTFACE);
	tfoffset = (layer == -1)? -1: em->fdata.layers[layer].offset;

	memset(&attribs, 0, sizeof(attribs));

	/* always use smooth shading even for flat faces, else vertex colors wont interpolate */
	glShadeModel(GL_SMOOTH);

	for (i=0,eve=em->verts.first; eve; eve= eve->next)
		eve->tmp.l = (intptr_t) i++;

#define PASSATTRIB(efa, eve, vert) {											\
	if(attribs.totorco) {														\
		float *orco = attribs.orco.array[eve->tmp.l];							\
		glVertexAttrib3fvARB(attribs.orco.glIndex, orco);						\
	}																			\
	for(b = 0; b < attribs.tottface; b++) {										\
		MTFace *_tf = (MTFace*)((char*)efa->data + attribs.tface[b].emOffset);	\
		glVertexAttrib2fvARB(attribs.tface[b].glIndex, _tf->uv[vert]);			\
	}																			\
	for(b = 0; b < attribs.totmcol; b++) {										\
		MCol *cp = (MCol*)((char*)efa->data + attribs.mcol[b].emOffset);		\
		GLubyte col[4];															\
		col[0]= cp->b; col[1]= cp->g; col[2]= cp->r; col[3]= cp->a;				\
		glVertexAttrib4ubvARB(attribs.mcol[b].glIndex, col);					\
	}																			\
	if(attribs.tottang) {														\
		float *tang = attribs.tang.array[i*4 + vert];							\
		glVertexAttrib3fvARB(attribs.tang.glIndex, tang);						\
	}																			\
}

	for (i=0,efa= em->faces.first; efa; i++,efa= efa->next) {
		int drawSmooth= (efa->flag & ME_SMOOTH);

		if(setDrawOptions && !setDrawOptions(userData, i))
			continue;

		new_matnr = efa->mat_nr + 1;
		if(new_matnr != matnr) {
			dodraw = setMaterial(matnr = new_matnr, &gattribs);
			if(dodraw)
				DM_vertex_attributes_from_gpu(dm, &gattribs, &attribs);
		}

		if(tfoffset != -1) {
			tf = (MTFace*)((char*)efa->data)+tfoffset;
			new_transp = tf->transp;

			if(new_transp != transp) {
				if(new_transp == GPU_BLEND_SOLID && orig_transp != GPU_BLEND_SOLID)
					GPU_set_material_blend_mode(orig_transp);
				else
					GPU_set_material_blend_mode(new_transp);
				transp = new_transp;
			}
		}

		if(dodraw) {
			glBegin(efa->v4?GL_QUADS:GL_TRIANGLES);
			if (!drawSmooth) {
				if(vertexCos) glNormal3fv(emdm->faceNos[i]);
				else glNormal3fv(efa->n);

				PASSATTRIB(efa, efa->v1, 0);
				if(vertexCos) glVertex3fv(vertexCos[(int) efa->v1->tmp.l]);
				else glVertex3fv(efa->v1->co);

				PASSATTRIB(efa, efa->v2, 1);
				if(vertexCos) glVertex3fv(vertexCos[(int) efa->v2->tmp.l]);
				else glVertex3fv(efa->v2->co);

				PASSATTRIB(efa, efa->v3, 2);
				if(vertexCos) glVertex3fv(vertexCos[(int) efa->v3->tmp.l]);
				else glVertex3fv(efa->v3->co);

				if(efa->v4) {
					PASSATTRIB(efa, efa->v4, 3);
					if(vertexCos) glVertex3fv(vertexCos[(int) efa->v4->tmp.l]);
					else glVertex3fv(efa->v4->co);
				}
			} else {
				PASSATTRIB(efa, efa->v1, 0);
				if(vertexCos) {
					glNormal3fv(vertexNos[(int) efa->v1->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v1->tmp.l]);
				}
				else {
					glNormal3fv(efa->v1->no);
					glVertex3fv(efa->v1->co);
				}

				PASSATTRIB(efa, efa->v2, 1);
				if(vertexCos) {
					glNormal3fv(vertexNos[(int) efa->v2->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v2->tmp.l]);
				}
				else {
					glNormal3fv(efa->v2->no);
					glVertex3fv(efa->v2->co);
				}

				PASSATTRIB(efa, efa->v3, 2);
				if(vertexCos) {
					glNormal3fv(vertexNos[(int) efa->v3->tmp.l]);
					glVertex3fv(vertexCos[(int) efa->v3->tmp.l]);
				}
				else {
					glNormal3fv(efa->v3->no);
					glVertex3fv(efa->v3->co);
				}

				if(efa->v4) {
					PASSATTRIB(efa, efa->v4, 3);
					if(vertexCos) {
						glNormal3fv(vertexNos[(int) efa->v4->tmp.l]);
						glVertex3fv(vertexCos[(int) efa->v4->tmp.l]);
					}
					else {
						glNormal3fv(efa->v4->no);
						glVertex3fv(efa->v4->co);
					}
				}
			}
			glEnd();
		}
	}
}

static void emDM_drawFacesGLSL(DerivedMesh *dm,
               int (*setMaterial)(int, void *attribs))
{
	dm->drawMappedFacesGLSL(dm, setMaterial, NULL, NULL);
}

static void emDM_getMinMax(DerivedMesh *dm, float min_r[3], float max_r[3])
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditVert *eve;
	int i;

	if (emdm->em->verts.first) {
		for (i=0,eve= emdm->em->verts.first; eve; i++,eve= eve->next) {
			if (emdm->vertexCos) {
				DO_MINMAX(emdm->vertexCos[i], min_r, max_r);
			} else {
				DO_MINMAX(eve->co, min_r, max_r);
			}
		}
	} else {
		min_r[0] = min_r[1] = min_r[2] = max_r[0] = max_r[1] = max_r[2] = 0.0;
	}
}
static int emDM_getNumVerts(DerivedMesh *dm)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;

	return BLI_countlist(&emdm->em->verts);
}

static int emDM_getNumEdges(DerivedMesh *dm)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;

	return BLI_countlist(&emdm->em->edges);
}

static int emDM_getNumFaces(DerivedMesh *dm)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;

	return BLI_countlist(&emdm->em->faces);
}

static void emDM_getVert(DerivedMesh *dm, int index, MVert *vert_r)
{
	EditVert *ev = ((EditMeshDerivedMesh *)dm)->em->verts.first;
	int i;

	for(i = 0; i < index; ++i) ev = ev->next;

	VECCOPY(vert_r->co, ev->co);

	vert_r->no[0] = ev->no[0] * 32767.0;
	vert_r->no[1] = ev->no[1] * 32767.0;
	vert_r->no[2] = ev->no[2] * 32767.0;

	/* TODO what to do with vert_r->flag and vert_r->mat_nr? */
	vert_r->mat_nr = 0;
	vert_r->bweight = (unsigned char) (ev->bweight*255.0f);
}

static void emDM_getEdge(DerivedMesh *dm, int index, MEdge *edge_r)
{
	EditMesh *em = ((EditMeshDerivedMesh *)dm)->em;
	EditEdge *ee = em->edges.first;
	EditVert *ev, *v1, *v2;
	int i;

	for(i = 0; i < index; ++i) ee = ee->next;

	edge_r->crease = (unsigned char) (ee->crease*255.0f);
	edge_r->bweight = (unsigned char) (ee->bweight*255.0f);
	/* TODO what to do with edge_r->flag? */
	edge_r->flag = ME_EDGEDRAW|ME_EDGERENDER;
	if (ee->seam) edge_r->flag |= ME_SEAM;
	if (ee->sharp) edge_r->flag |= ME_SHARP;
#if 0
	/* this needs setup of f2 field */
	if (!ee->f2) edge_r->flag |= ME_LOOSEEDGE;
#endif

	/* goddamn, we have to search all verts to find indices */
	v1 = ee->v1;
	v2 = ee->v2;
	for(i = 0, ev = em->verts.first; v1 || v2; i++, ev = ev->next) {
		if(ev == v1) {
			edge_r->v1 = i;
			v1 = NULL;
		}
		if(ev == v2) {
			edge_r->v2 = i;
			v2 = NULL;
		}
	}
}

static void emDM_getFace(DerivedMesh *dm, int index, MFace *face_r)
{
	EditMesh *em = ((EditMeshDerivedMesh *)dm)->em;
	EditFace *ef = em->faces.first;
	EditVert *ev, *v1, *v2, *v3, *v4;
	int i;

	for(i = 0; i < index; ++i) ef = ef->next;

	face_r->mat_nr = ef->mat_nr;
	face_r->flag = ef->flag;

	/* goddamn, we have to search all verts to find indices */
	v1 = ef->v1;
	v2 = ef->v2;
	v3 = ef->v3;
	v4 = ef->v4;
	if(!v4) face_r->v4 = 0;

	for(i = 0, ev = em->verts.first; v1 || v2 || v3 || v4;
	    i++, ev = ev->next) {
		if(ev == v1) {
			face_r->v1 = i;
			v1 = NULL;
		}
		if(ev == v2) {
			face_r->v2 = i;
			v2 = NULL;
		}
		if(ev == v3) {
			face_r->v3 = i;
			v3 = NULL;
		}
		if(ev == v4) {
			face_r->v4 = i;
			v4 = NULL;
		}
	}

	test_index_face(face_r, NULL, 0, ef->v4?4:3);
}

static void emDM_copyVertArray(DerivedMesh *dm, MVert *vert_r)
{
	EditVert *ev = ((EditMeshDerivedMesh *)dm)->em->verts.first;

	for( ; ev; ev = ev->next, ++vert_r) {
		VECCOPY(vert_r->co, ev->co);

		vert_r->no[0] = ev->no[0] * 32767.0;
		vert_r->no[1] = ev->no[1] * 32767.0;
		vert_r->no[2] = ev->no[2] * 32767.0;

		/* TODO what to do with vert_r->flag and vert_r->mat_nr? */
		vert_r->mat_nr = 0;
		vert_r->flag = 0;
		vert_r->bweight = (unsigned char) (ev->bweight*255.0f);
	}
}

static void emDM_copyEdgeArray(DerivedMesh *dm, MEdge *edge_r)
{
	EditMesh *em = ((EditMeshDerivedMesh *)dm)->em;
	EditEdge *ee = em->edges.first;
	EditVert *ev;
	int i;

	/* store vertex indices in tmp union */
	for(ev = em->verts.first, i = 0; ev; ev = ev->next, ++i)
		ev->tmp.l = (intptr_t) i;

	for( ; ee; ee = ee->next, ++edge_r) {
		edge_r->crease = (unsigned char) (ee->crease*255.0f);
		edge_r->bweight = (unsigned char) (ee->bweight*255.0f);
		/* TODO what to do with edge_r->flag? */
		edge_r->flag = ME_EDGEDRAW|ME_EDGERENDER;
		if (ee->seam) edge_r->flag |= ME_SEAM;
		if (ee->sharp) edge_r->flag |= ME_SHARP;
#if 0
		/* this needs setup of f2 field */
		if (!ee->f2) edge_r->flag |= ME_LOOSEEDGE;
#endif

		edge_r->v1 = (int)ee->v1->tmp.l;
		edge_r->v2 = (int)ee->v2->tmp.l;
	}
}

static void emDM_copyFaceArray(DerivedMesh *dm, MFace *face_r)
{
	EditMesh *em = ((EditMeshDerivedMesh *)dm)->em;
	EditFace *ef = em->faces.first;
	EditVert *ev;
	int i;

	/* store vertexes indices in tmp union */
	for(ev = em->verts.first, i = 0; ev; ev = ev->next, ++i)
		ev->tmp.l = (intptr_t) i;

	for( ; ef; ef = ef->next, ++face_r) {
		face_r->mat_nr = ef->mat_nr;
		face_r->flag = ef->flag;

		face_r->v1 = (int)ef->v1->tmp.l;
		face_r->v2 = (int)ef->v2->tmp.l;
		face_r->v3 = (int)ef->v3->tmp.l;
		if(ef->v4) face_r->v4 = (int)ef->v4->tmp.l;
		else face_r->v4 = 0;

		test_index_face(face_r, NULL, 0, ef->v4?4:3);
	}
}

static void *emDM_getFaceDataArray(DerivedMesh *dm, int type)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;
	EditMesh *em= emdm->em;
	EditFace *efa;
	char *data, *emdata;
	void *datalayer;
	int index, offset, size;

	datalayer = DM_get_face_data_layer(dm, type);
	if(datalayer)
		return datalayer;

	/* layers are store per face for editmesh, we convert to a temporary
	 * data layer array in the derivedmesh when these are requested */
	if(type == CD_MTFACE || type == CD_MCOL) {
		index = CustomData_get_layer_index(&em->fdata, type);

		if(index != -1) {
			offset = em->fdata.layers[index].offset;
			size = CustomData_sizeof(type);

			DM_add_face_layer(dm, type, CD_CALLOC, NULL);
			index = CustomData_get_layer_index(&dm->faceData, type);
			dm->faceData.layers[index].flag |= CD_FLAG_TEMPORARY;

			data = datalayer = DM_get_face_data_layer(dm, type);
			for(efa=em->faces.first; efa; efa=efa->next, data+=size) {
				emdata = CustomData_em_get(&em->fdata, efa->data, type);
				memcpy(data, emdata, size);
			}
		}
	}

	return datalayer;
}

static void emDM_release(DerivedMesh *dm)
{
	EditMeshDerivedMesh *emdm= (EditMeshDerivedMesh*) dm;

	if (DM_release(dm)) {
		if (emdm->vertexCos) {
			MEM_freeN(emdm->vertexCos);
			MEM_freeN(emdm->vertexNos);
			MEM_freeN(emdm->faceNos);
		}

		MEM_freeN(emdm);
	}
}

static DerivedMesh *getEditMeshDerivedMesh(EditMesh *em, Object *ob,
                                           float (*vertexCos)[3])
{
	EditMeshDerivedMesh *emdm = MEM_callocN(sizeof(*emdm), "emdm");

	DM_init(&emdm->dm, BLI_countlist(&em->verts),
	                 BLI_countlist(&em->edges), BLI_countlist(&em->faces));

	emdm->dm.getMinMax = emDM_getMinMax;

	emdm->dm.getNumVerts = emDM_getNumVerts;
	emdm->dm.getNumEdges = emDM_getNumEdges;
	emdm->dm.getNumFaces = emDM_getNumFaces;

	emdm->dm.getVert = emDM_getVert;
	emdm->dm.getEdge = emDM_getEdge;
	emdm->dm.getFace = emDM_getFace;
	emdm->dm.copyVertArray = emDM_copyVertArray;
	emdm->dm.copyEdgeArray = emDM_copyEdgeArray;
	emdm->dm.copyFaceArray = emDM_copyFaceArray;
	emdm->dm.getFaceDataArray = emDM_getFaceDataArray;

	emdm->dm.foreachMappedVert = emDM_foreachMappedVert;
	emdm->dm.foreachMappedEdge = emDM_foreachMappedEdge;
	emdm->dm.foreachMappedFaceCenter = emDM_foreachMappedFaceCenter;

	emdm->dm.drawEdges = emDM_drawEdges;
	emdm->dm.drawMappedEdges = emDM_drawMappedEdges;
	emdm->dm.drawMappedEdgesInterp = emDM_drawMappedEdgesInterp;
	emdm->dm.drawMappedFaces = emDM_drawMappedFaces;
	emdm->dm.drawMappedFacesTex = emDM_drawMappedFacesTex;
	emdm->dm.drawMappedFacesGLSL = emDM_drawMappedFacesGLSL;
	emdm->dm.drawFacesTex = emDM_drawFacesTex;
	emdm->dm.drawFacesGLSL = emDM_drawFacesGLSL;
	emdm->dm.drawUVEdges = emDM_drawUVEdges;

	emdm->dm.release = emDM_release;
	
	emdm->em = em;
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
		EditVert *eve;
		EditFace *efa;
		int totface = BLI_countlist(&em->faces);
		int i;

		for (i=0,eve=em->verts.first; eve; eve= eve->next)
			eve->tmp.l = (intptr_t) i++;

		emdm->vertexNos = MEM_callocN(sizeof(*emdm->vertexNos)*i, "emdm_vno");
		emdm->faceNos = MEM_mallocN(sizeof(*emdm->faceNos)*totface, "emdm_vno");

		for(i=0, efa= em->faces.first; efa; i++, efa=efa->next) {
			float *v1 = vertexCos[(int) efa->v1->tmp.l];
			float *v2 = vertexCos[(int) efa->v2->tmp.l];
			float *v3 = vertexCos[(int) efa->v3->tmp.l];
			float *no = emdm->faceNos[i];
			
			if(efa->v4) {
				float *v4 = vertexCos[(int) efa->v4->tmp.l];

				CalcNormFloat4(v1, v2, v3, v4, no);
				VecAddf(emdm->vertexNos[(int) efa->v4->tmp.l], emdm->vertexNos[(int) efa->v4->tmp.l], no);
			}
			else {
				CalcNormFloat(v1, v2, v3, no);
			}

			VecAddf(emdm->vertexNos[(int) efa->v1->tmp.l], emdm->vertexNos[(int) efa->v1->tmp.l], no);
			VecAddf(emdm->vertexNos[(int) efa->v2->tmp.l], emdm->vertexNos[(int) efa->v2->tmp.l], no);
			VecAddf(emdm->vertexNos[(int) efa->v3->tmp.l], emdm->vertexNos[(int) efa->v3->tmp.l], no);
		}

		for(i=0, eve= em->verts.first; eve; i++, eve=eve->next) {
			float *no = emdm->vertexNos[i];
			/* following Mesh convention; we use vertex coordinate itself
			 * for normal in this case */
			if (Normalize(no)==0.0) {
				VECCOPY(no, vertexCos[i]);
				Normalize(no);
			}
		}
	}

	return (DerivedMesh*) emdm;
}

/***/

DerivedMesh *mesh_create_derived_for_modifier(Scene *scene, Object *ob, ModifierData *md)
{
	Mesh *me = ob->data;
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	DerivedMesh *dm;

	md->scene= scene;
	
	if (!(md->mode&eModifierMode_Realtime)) return NULL;
	if (mti->isDisabled && mti->isDisabled(md)) return NULL;

	if (mti->type==eModifierTypeType_OnlyDeform) {
		int numVerts;
		float (*deformedVerts)[3] = mesh_getVertexCos(me, &numVerts);

		mti->deformVerts(md, ob, NULL, deformedVerts, numVerts, 0, 0);
		dm = getMeshDerivedMesh(me, ob, deformedVerts);

		MEM_freeN(deformedVerts);
	} else {
		DerivedMesh *tdm = getMeshDerivedMesh(me, ob, NULL);
		dm = mti->applyModifier(md, ob, tdm, 0, 0);

		if(tdm != dm) tdm->release(tdm);
	}

	return dm;
}

static float *get_editmesh_orco_verts(EditMesh *em)
{
	EditVert *eve;
	float *orco;
	int a, totvert;

	/* these may not really be the orco's, but it's only for preview.
	 * could be solver better once, but isn't simple */

	totvert= 0;
	for(eve=em->verts.first; eve; eve=eve->next)
		totvert++;
	
	orco = MEM_mallocN(sizeof(float)*3*totvert, "EditMesh Orco");

	for(a=0, eve=em->verts.first; eve; eve=eve->next, a+=3)
		VECCOPY(orco+a, eve->co);
	
	return orco;
}

/* orco custom data layer */

static DerivedMesh *create_orco_dm(Object *ob, Mesh *me, EditMesh *em)
{
	DerivedMesh *dm;
	float (*orco)[3];

	if(em) {
		dm= CDDM_from_editmesh(em, me);
		orco= (float(*)[3])get_editmesh_orco_verts(em);
	}
	else {
		dm= CDDM_from_mesh(me, ob);
		orco= (float(*)[3])get_mesh_orco_verts(ob);
	}

	CDDM_apply_vert_coords(dm, orco);
	CDDM_calc_normals(dm);
	MEM_freeN(orco);

	return dm;
}

static void add_orco_dm(Object *ob, EditMesh *em, DerivedMesh *dm, DerivedMesh *orcodm)
{
	float (*orco)[3], (*layerorco)[3];
	int totvert;

	totvert= dm->getNumVerts(dm);

	if(orcodm) {
		orco= MEM_callocN(sizeof(float)*3*totvert, "dm orco");

		if(orcodm->getNumVerts(orcodm) == totvert)
			orcodm->getVertCos(orcodm, orco);
		else
			dm->getVertCos(dm, orco);
	}
	else {
		if(em) orco= (float(*)[3])get_editmesh_orco_verts(em);
		else orco= (float(*)[3])get_mesh_orco_verts(ob);
	}

	transform_mesh_orco_verts(ob->data, orco, totvert, 0);

	if((layerorco = DM_get_vert_data_layer(dm, CD_ORCO))) {
		memcpy(layerorco, orco, sizeof(float)*totvert);
		MEM_freeN(orco);
	}
	else
		DM_add_vert_layer(dm, CD_ORCO, CD_ASSIGN, orco);
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
	Mesh *me = ob->data;
	MFace *mf = me->mface;
	ColorBand *coba= stored_cb;	/* warning, not a local var */
	unsigned char *wtcol;
	int i;
	
	wtcol = MEM_callocN (sizeof (unsigned char) * me->totface*4*4, "weightmap");
	
	memset(wtcol, 0x55, sizeof (unsigned char) * me->totface*4*4);
	for (i=0; i<me->totface; i++, mf++) {
		calc_weightpaint_vert_color(ob, coba, mf->v1, &wtcol[(i*4 + 0)*4]); 
		calc_weightpaint_vert_color(ob, coba, mf->v2, &wtcol[(i*4 + 1)*4]); 
		calc_weightpaint_vert_color(ob, coba, mf->v3, &wtcol[(i*4 + 2)*4]); 
		if (mf->v4)
			calc_weightpaint_vert_color(ob, coba, mf->v4, &wtcol[(i*4 + 3)*4]); 
	}
	
	CustomData_add_layer(&dm->faceData, CD_WEIGHT_MCOL, CD_ASSIGN, wtcol, dm->numFaceData);
}

/* new value for useDeform -1  (hack for the gameengine):
 * - apply only the modifier stack of the object, skipping the virtual modifiers,
 * - don't apply the key
 * - apply deform modifiers and input vertexco
 */
static void mesh_calc_modifiers(Scene *scene, Object *ob, float (*inputVertexCos)[3],
                                DerivedMesh **deform_r, DerivedMesh **final_r,
                                int useRenderParams, int useDeform,
                                int needMapping, CustomDataMask dataMask, int index)
{
	Mesh *me = ob->data;
	ModifierData *firstmd, *md;
	LinkNode *datamasks, *curr;
	CustomDataMask mask;
	float (*deformedVerts)[3] = NULL;
	DerivedMesh *dm, *orcodm, *finaldm;
	int numVerts = me->totvert;
	int required_mode;

	md = firstmd = (useDeform<0) ? ob->modifiers.first : modifiers_getVirtualModifierList(ob);

	modifiers_clearErrors(ob);

	if(useRenderParams) required_mode = eModifierMode_Render;
	else required_mode = eModifierMode_Realtime;

	/* we always want to keep original indices */
	dataMask |= CD_MASK_ORIGINDEX;

	datamasks = modifiers_calcDataMasks(ob, md, dataMask, required_mode);
	curr = datamasks;

	if(deform_r) *deform_r = NULL;
	*final_r = NULL;

	if(useDeform) {
		if(useDeform > 0 && do_ob_key(scene, ob)) /* shape key makes deform verts */
			deformedVerts = mesh_getVertexCos(me, &numVerts);
		else if(inputVertexCos)
			deformedVerts = inputVertexCos;
		
		/* Apply all leading deforming modifiers */
		for(;md; md = md->next, curr = curr->next) {
			ModifierTypeInfo *mti = modifierType_getInfo(md->type);

			md->scene= scene;
			
			if(!modifier_isEnabled(md, required_mode)) continue;
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
			deformedVerts = mesh_getRefKeyCos(me, &numVerts);
	}


	/* Now apply all remaining modifiers. If useDeform is off then skip
	 * OnlyDeform ones. 
	 */
	dm = NULL;
	orcodm = NULL;

	for(;md; md = md->next, curr = curr->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		md->scene= scene;
		
		if(!modifier_isEnabled(md, required_mode)) continue;
		if(mti->type == eModifierTypeType_OnlyDeform && !useDeform) continue;
		if((mti->flags & eModifierTypeFlag_RequiresOriginalData) && dm) {
			modifier_setError(md, "Modifier requires original data, bad stack position.");
			continue;
		}
		if(needMapping && !modifier_supportsMapping(md)) continue;
		if(useDeform < 0 && mti->dependsOnTime && mti->dependsOnTime(md)) continue;

		/* add an orco layer if needed by this modifier */
		if(dm && mti->requiredDataMask) {
			mask = mti->requiredDataMask(ob, md);
			if(mask & CD_MASK_ORCO)
				add_orco_dm(ob, NULL, dm, orcodm);
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
					deformedVerts = mesh_getVertexCos(me, &numVerts);
				}
			}

			mti->deformVerts(md, ob, dm, deformedVerts, numVerts, useRenderParams, useDeform);
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

				if(dataMask & CD_MASK_WEIGHT_MCOL)
					add_weight_mcol_dm(ob, dm);
			}

			/* create an orco derivedmesh in parallel */
			mask= (CustomDataMask)GET_INT_FROM_POINTER(curr->link);
			if(mask & CD_MASK_ORCO) {
				if(!orcodm)
					orcodm= create_orco_dm(ob, me, NULL);

				mask &= ~CD_MASK_ORCO;
				DM_set_only_copy(orcodm, mask);
				ndm = mti->applyModifier(md, ob, orcodm, useRenderParams, !inputVertexCos);

				if(ndm) {
					/* if the modifier returned a new dm, release the old one */
					if(orcodm && orcodm != ndm) orcodm->release(orcodm);
					orcodm = ndm;
				}
			}

			/* set the DerivedMesh to only copy needed data */
			DM_set_only_copy(dm, mask);
			
			/* add an origspace layer if needed */
			if(((CustomDataMask)GET_INT_FROM_POINTER(curr->link)) & CD_MASK_ORIGSPACE)
				if(!CustomData_has_layer(&dm->faceData, CD_ORIGSPACE))
					DM_add_face_layer(dm, CD_ORIGSPACE, CD_DEFAULT, NULL);

			ndm = mti->applyModifier(md, ob, dm, useRenderParams, !inputVertexCos);

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
		
		/* grab modifiers until index i */
		if((index >= 0) && (modifiers_indexInObject(ob, md) >= index))
			break;
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

		if(dataMask & CD_MASK_WEIGHT_MCOL)
			add_weight_mcol_dm(ob, finaldm);
	} else if(dm) {
		finaldm = dm;
	} else {
		finaldm = CDDM_from_mesh(me, ob);

		if(deformedVerts) {
			CDDM_apply_vert_coords(finaldm, deformedVerts);
			CDDM_calc_normals(finaldm);
		}

		if(dataMask & CD_MASK_WEIGHT_MCOL)
			add_weight_mcol_dm(ob, finaldm);
	}

	/* add an orco layer if needed */
	if(dataMask & CD_MASK_ORCO) {
		add_orco_dm(ob, NULL, finaldm, orcodm);

		if(deform_r && *deform_r)
			add_orco_dm(ob, NULL, *deform_r, NULL);
	}

	*final_r = finaldm;

	if(orcodm)
		orcodm->release(orcodm);

	if(deformedVerts && deformedVerts != inputVertexCos)
		MEM_freeN(deformedVerts);

	BLI_linklist_free(datamasks, NULL);
}

static float (*editmesh_getVertexCos(EditMesh *em, int *numVerts_r))[3]
{
	int i, numVerts = *numVerts_r = BLI_countlist(&em->verts);
	float (*cos)[3];
	EditVert *eve;

	cos = MEM_mallocN(sizeof(*cos)*numVerts, "vertexcos");
	for (i=0,eve=em->verts.first; i<numVerts; i++,eve=eve->next) {
		VECCOPY(cos[i], eve->co);
	}

	return cos;
}

static int editmesh_modifier_is_enabled(ModifierData *md, DerivedMesh *dm)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;

	if(!modifier_isEnabled(md, required_mode)) return 0;
	if((mti->flags & eModifierTypeFlag_RequiresOriginalData) && dm) {
		modifier_setError(md, "Modifier requires original data, bad stack position.");
		return 0;
	}
	
	return 1;
}

static void editmesh_calc_modifiers(Scene *scene, Object *ob, EditMesh *em, DerivedMesh **cage_r,
                                    DerivedMesh **final_r,
                                    CustomDataMask dataMask)
{
	ModifierData *md;
	float (*deformedVerts)[3] = NULL;
	CustomDataMask mask;
	DerivedMesh *dm, *orcodm = NULL;
	int i, numVerts = 0, cageIndex = modifiers_getCageIndex(ob, NULL);
	LinkNode *datamasks, *curr;
	int required_mode = eModifierMode_Realtime | eModifierMode_Editmode;

	modifiers_clearErrors(ob);

	if(cage_r && cageIndex == -1) {
		*cage_r = getEditMeshDerivedMesh(em, ob, NULL);
	}

	dm = NULL;
	md = ob->modifiers.first;
	
	/* we always want to keep original indices */
	dataMask |= CD_MASK_ORIGINDEX;

	datamasks = modifiers_calcDataMasks(ob, md, dataMask, required_mode);

	curr = datamasks;
	for(i = 0; md; i++, md = md->next, curr = curr->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		md->scene= scene;
		
		if(!editmesh_modifier_is_enabled(md, dm))
			continue;

		/* add an orco layer if needed by this modifier */
		if(dm && mti->requiredDataMask) {
			mask = mti->requiredDataMask(ob, md);
			if(mask & CD_MASK_ORCO)
				add_orco_dm(ob, em, dm, orcodm);
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

			/* create an orco derivedmesh in parallel */
			mask= (CustomDataMask)GET_INT_FROM_POINTER(curr->link);
			if(mask & CD_MASK_ORCO) {
				if(!orcodm)
					orcodm= create_orco_dm(ob, ob->data, em);

				mask &= ~CD_MASK_ORCO;
				DM_set_only_copy(orcodm, mask);
				ndm = mti->applyModifierEM(md, ob, em, orcodm);

				if(ndm) {
					/* if the modifier returned a new dm, release the old one */
					if(orcodm && orcodm != ndm) orcodm->release(orcodm);
					orcodm = ndm;
				}
			}

			/* set the DerivedMesh to only copy needed data */
			DM_set_only_copy(dm, (CustomDataMask)GET_INT_FROM_POINTER(curr->link));

			if(((CustomDataMask)GET_INT_FROM_POINTER(curr->link)) & CD_MASK_ORIGSPACE)
				if(!CustomData_has_layer(&dm->faceData, CD_ORIGSPACE))
					DM_add_face_layer(dm, CD_ORIGSPACE, CD_DEFAULT, NULL);
			
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
	} else if (!deformedVerts && cage_r && *cage_r) {
		*final_r = *cage_r;
	} else {
		*final_r = getEditMeshDerivedMesh(em, ob, deformedVerts);
		deformedVerts = NULL;
	}

	/* add an orco layer if needed */
	if(dataMask & CD_MASK_ORCO)
		add_orco_dm(ob, em, *final_r, orcodm);

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
}

static void mesh_build_data(Scene *scene, Object *ob, CustomDataMask dataMask)
{
	Object *obact = scene->basact?scene->basact->object:NULL;
	int editing = paint_facesel_test(ob);
	int needMapping = editing && (ob==obact);
	float min[3], max[3];
	
	clear_mesh_caches(ob);

	mesh_calc_modifiers(scene, ob, NULL, &ob->derivedDeform,
						&ob->derivedFinal, 0, 1,
						needMapping, dataMask, -1);

	INIT_MINMAX(min, max);

	ob->derivedFinal->getMinMax(ob->derivedFinal, min, max);

	if(!ob->bb)
		ob->bb= MEM_callocN(sizeof(BoundBox), "bb");
	boundbox_set_from_min_max(ob->bb, min, max);

	ob->derivedFinal->needsFree = 0;
	ob->derivedDeform->needsFree = 0;
	ob->lastDataMask = dataMask;

}

static void editmesh_build_data(Scene *scene, Object *obedit, EditMesh *em, CustomDataMask dataMask)
{
	float min[3], max[3];

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

	editmesh_calc_modifiers(scene, obedit, em, &em->derivedCage, &em->derivedFinal, dataMask);
	em->lastDataMask = dataMask;

	INIT_MINMAX(min, max);

	em->derivedFinal->getMinMax(em->derivedFinal, min, max);

	if(!obedit->bb)
		obedit->bb= MEM_callocN(sizeof(BoundBox), "bb");
	boundbox_set_from_min_max(obedit->bb, min, max);

	em->derivedFinal->needsFree = 0;
	em->derivedCage->needsFree = 0;
}

void makeDerivedMesh(Scene *scene, Object *ob, EditMesh *em, CustomDataMask dataMask)
{
	if (em) {
		editmesh_build_data(scene, ob, em, dataMask);
	} else {
		mesh_build_data(scene, ob, dataMask);
	}
}

/***/

DerivedMesh *mesh_get_derived_final(Scene *scene, Object *ob, CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	if(!ob->derivedFinal || (dataMask & ob->lastDataMask) != dataMask)
		mesh_build_data(scene, ob, dataMask);

	return ob->derivedFinal;
}

DerivedMesh *mesh_get_derived_deform(Scene *scene, Object *ob, CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	if(!ob->derivedDeform || (dataMask & ob->lastDataMask) != dataMask)
		mesh_build_data(scene, ob, dataMask);

	return ob->derivedDeform;
}

DerivedMesh *mesh_create_derived_render(Scene *scene, Object *ob, CustomDataMask dataMask)
{
	DerivedMesh *final;
	
	mesh_calc_modifiers(scene, ob, NULL, NULL, &final, 1, 1, 0, dataMask, -1);

	return final;
}

DerivedMesh *mesh_create_derived_index_render(Scene *scene, Object *ob, CustomDataMask dataMask, int index)
{
	DerivedMesh *final;
	
	mesh_calc_modifiers(scene, ob, NULL, NULL, &final, 1, 1, 0, dataMask, index);

	return final;
}

DerivedMesh *mesh_create_derived_view(Scene *scene, Object *ob, CustomDataMask dataMask)
{
	DerivedMesh *final;

	mesh_calc_modifiers(scene, ob, NULL, NULL, &final, 0, 1, 0, dataMask, -1);

	return final;
}

DerivedMesh *mesh_create_derived_no_deform(Scene *scene, Object *ob, float (*vertCos)[3],
                                           CustomDataMask dataMask)
{
	DerivedMesh *final;
	
	mesh_calc_modifiers(scene, ob, vertCos, NULL, &final, 0, 0, 0, dataMask, -1);

	return final;
}

DerivedMesh *mesh_create_derived_no_virtual(Scene *scene, Object *ob, float (*vertCos)[3],
                                            CustomDataMask dataMask)
{
	DerivedMesh *final;
	
	mesh_calc_modifiers(scene, ob, vertCos, NULL, &final, 0, -1, 0, dataMask, -1);

	return final;
}

DerivedMesh *mesh_create_derived_no_deform_render(Scene *scene, Object *ob,
                                                  float (*vertCos)[3],
                                                  CustomDataMask dataMask)
{
	DerivedMesh *final;

	mesh_calc_modifiers(scene, ob, vertCos, NULL, &final, 1, 0, 0, dataMask, -1);

	return final;
}

/***/

DerivedMesh *editmesh_get_derived_cage_and_final(Scene *scene, Object *obedit, EditMesh *em, DerivedMesh **final_r,
                                                 CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	if(!em->derivedCage ||
	   (em->lastDataMask & dataMask) != dataMask)
		editmesh_build_data(scene, obedit, em, dataMask);

	*final_r = em->derivedFinal;
	return em->derivedCage;
}

DerivedMesh *editmesh_get_derived_cage(Scene *scene, Object *obedit, EditMesh *em, CustomDataMask dataMask)
{
	/* if there's no derived mesh or the last data mask used doesn't include
	 * the data we need, rebuild the derived mesh
	 */
	if(!em->derivedCage ||
	   (em->lastDataMask & dataMask) != dataMask)
		editmesh_build_data(scene, obedit, em, dataMask);

	return em->derivedCage;
}

DerivedMesh *editmesh_get_derived_base(Object *obedit, EditMesh *em)
{
	return getEditMeshDerivedMesh(em, obedit, NULL);
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
	
	dm= mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
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

/* ********* crazyspace *************** */

int editmesh_get_first_deform_matrices(Object *ob, EditMesh *em, float (**deformmats)[3][3], float (**deformcos)[3])
{
	ModifierData *md;
	DerivedMesh *dm;
	int i, a, numleft = 0, numVerts = 0;
	int cageIndex = modifiers_getCageIndex(ob, NULL);
	float (*defmats)[3][3] = NULL, (*deformedVerts)[3] = NULL;

	modifiers_clearErrors(ob);

	dm = NULL;
	md = ob->modifiers.first;

	/* compute the deformation matrices and coordinates for the first
	   modifiers with on cage editing that are enabled and support computing
	   deform matrices */
	for(i = 0; md && i <= cageIndex; i++, md = md->next) {
		ModifierTypeInfo *mti = modifierType_getInfo(md->type);

		if(!editmesh_modifier_is_enabled(md, dm))
			continue;

		if(mti->type==eModifierTypeType_OnlyDeform && mti->deformMatricesEM) {
			if(!defmats) {
				dm= getEditMeshDerivedMesh(em, ob, NULL);
				deformedVerts= editmesh_getVertexCos(em, &numVerts);
				defmats= MEM_callocN(sizeof(*defmats)*numVerts, "defmats");

				for(a=0; a<numVerts; a++)
					Mat3One(defmats[a]);
			}

			mti->deformMatricesEM(md, ob, em, dm, deformedVerts, defmats,
				numVerts);
		}
		else
			break;
	}

	for(; md && i <= cageIndex; md = md->next, i++)
		if(editmesh_modifier_is_enabled(md, dm) && modifier_isDeformer(md))
			numleft++;

	if(dm)
		dm->release(dm);
	
	*deformmats= defmats;
	*deformcos= deformedVerts;

	return numleft;
}

/* ******************* GLSL ******************** */

void DM_add_tangent_layer(DerivedMesh *dm)
{
	/* mesh vars */
	MTFace *mtface, *tf;
	MFace *mface, *mf;
	MVert *mvert, *v1, *v2, *v3, *v4;
	MemArena *arena= NULL;
	VertexTangent **vtangents= NULL;
	float (*orco)[3]= NULL, (*tangent)[3];
	float *uv1, *uv2, *uv3, *uv4, *vtang;
	float fno[3], tang[3], uv[4][2];
	int i, j, len, mf_vi[4], totvert, totface;

	if(CustomData_get_layer_index(&dm->faceData, CD_TANGENT) != -1)
		return;

	/* check we have all the needed layers */
	totvert= dm->getNumVerts(dm);
	totface= dm->getNumFaces(dm);

	mvert= dm->getVertArray(dm);
	mface= dm->getFaceArray(dm);
	mtface= dm->getFaceDataArray(dm, CD_MTFACE);

	if(!mtface) {
		orco= dm->getVertDataArray(dm, CD_ORCO);
		if(!orco)
			return;
	}
	
	/* create tangent layer */
	DM_add_face_layer(dm, CD_TANGENT, CD_CALLOC, NULL);
	tangent= DM_get_face_data_layer(dm, CD_TANGENT);
	
	/* allocate some space */
	arena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	BLI_memarena_use_calloc(arena);
	vtangents= MEM_callocN(sizeof(VertexTangent*)*totvert, "VertexTangent");
	
	/* sum tangents at connected vertices */
	for(i=0, tf=mtface, mf=mface; i < totface; mf++, tf++, i++) {
		v1= &mvert[mf->v1];
		v2= &mvert[mf->v2];
		v3= &mvert[mf->v3];

		if (mf->v4) {
			v4= &mvert[mf->v4];
			CalcNormFloat4(v4->co, v3->co, v2->co, v1->co, fno);
		}
		else {
			v4= NULL;
			CalcNormFloat(v3->co, v2->co, v1->co, fno);
		}
		
		if(mtface) {
			uv1= tf->uv[0];
			uv2= tf->uv[1];
			uv3= tf->uv[2];
			uv4= tf->uv[3];
		}
		else {
			uv1= uv[0]; uv2= uv[1]; uv3= uv[2]; uv4= uv[3];
			spheremap(orco[mf->v1][0], orco[mf->v1][1], orco[mf->v1][2], &uv[0][0], &uv[0][1]);
			spheremap(orco[mf->v2][0], orco[mf->v2][1], orco[mf->v2][2], &uv[1][0], &uv[1][1]);
			spheremap(orco[mf->v3][0], orco[mf->v3][1], orco[mf->v3][2], &uv[2][0], &uv[2][1]);
			if(v4)
				spheremap(orco[mf->v4][0], orco[mf->v4][1], orco[mf->v4][2], &uv[3][0], &uv[3][1]);
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
		
		if(mtface) {
			uv1= tf->uv[0];
			uv2= tf->uv[1];
			uv3= tf->uv[2];
			uv4= tf->uv[3];
		}
		else {
			uv1= uv[0]; uv2= uv[1]; uv3= uv[2]; uv4= uv[3];
			spheremap(orco[mf->v1][0], orco[mf->v1][1], orco[mf->v1][2], &uv[0][0], &uv[0][1]);
			spheremap(orco[mf->v2][0], orco[mf->v2][1], orco[mf->v2][2], &uv[1][0], &uv[1][1]);
			spheremap(orco[mf->v3][0], orco[mf->v3][1], orco[mf->v3][2], &uv[2][0], &uv[2][1]);
			if(len==4)
				spheremap(orco[mf->v4][0], orco[mf->v4][1], orco[mf->v4][2], &uv[3][0], &uv[3][1]);
		}
		
		mf_vi[0]= mf->v1;
		mf_vi[1]= mf->v2;
		mf_vi[2]= mf->v3;
		mf_vi[3]= mf->v4;
		
		for(j=0; j<len; j++) {
			vtang= find_vertex_tangent(vtangents[mf_vi[j]], mtface ? tf->uv[j] : uv[j]);

			VECCOPY(tangent[j], vtang);
			Normalize(tangent[j]);
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
	fdata = &dm->faceData;

	/* ugly hack, editmesh derivedmesh doesn't copy face data, this way we
	 * can use offsets instead */
	if(dm->release == emDM_release)
		tfdata = &((EditMeshDerivedMesh*)dm)->em->fdata;
	else
		tfdata = fdata;

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
			}
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
			}
		}
	}
}

