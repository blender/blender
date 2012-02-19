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
 * The Original Code is Copyright (C) 2011 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Benoit Bolsee,
 *                 Nick Samarin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_ID.h"

#include "BKE_library.h"
#include "BKE_depsgraph.h"
#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_math_vector.h"
#include "BLI_linklist.h"

#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_screen.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "mesh_intern.h"
#include "recast-capi.h"

static void createVertsTrisData(bContext *C, LinkNode* obs, int *nverts_r, float **verts_r, int *ntris_r, int **tris_r)
{
	MVert *mvert;
	int nfaces= 0, *tri, i, curnverts, basenverts, curnfaces;
	MFace *mface;
	float co[3], wco[3];
	Object *ob;
	LinkNode *oblink, *dmlink;
	DerivedMesh *dm;
	Scene* scene= CTX_data_scene(C);
	LinkNode* dms= NULL;

	int nverts, ntris, *tris;
	float *verts;

	nverts= 0;
	ntris= 0;

	/* calculate number of verts and tris */
	for(oblink= obs; oblink; oblink= oblink->next) {
		ob= (Object*) oblink->link;
		dm= mesh_create_derived_no_virtual(scene, ob, NULL, CD_MASK_MESH);
		BLI_linklist_append(&dms, (void*)dm);

		nverts+= dm->getNumVerts(dm);
		nfaces= dm->getNumTessFaces(dm);
		ntris+= nfaces;

		/* resolve quad faces */
		mface= dm->getTessFaceArray(dm);
		for(i= 0; i<nfaces; i++) {
			MFace* mf= &mface[i];
			if(mf->v4)
				ntris+=1;
		}
	}

	/* create data */
	verts= MEM_mallocN(sizeof(float)*3*nverts, "createVertsTrisData verts");
	tris= MEM_mallocN(sizeof(int)*3*ntris, "createVertsTrisData faces");

	basenverts= 0;
	tri= tris;
	for(oblink= obs, dmlink= dms; oblink && dmlink;
			oblink= oblink->next, dmlink= dmlink->next) {
		ob= (Object*) oblink->link;
		dm= (DerivedMesh*) dmlink->link;

		curnverts= dm->getNumVerts(dm);
		mvert= dm->getVertArray(dm);

		/* copy verts */
		for(i= 0; i<curnverts; i++) {
			MVert *v= &mvert[i];

			copy_v3_v3(co, v->co);
			mul_v3_m4v3(wco, ob->obmat, co);

			verts[3*(basenverts+i)+0]= wco[0];
			verts[3*(basenverts+i)+1]= wco[2];
			verts[3*(basenverts+i)+2]= wco[1];
		}

		/* create tris */
		curnfaces= dm->getNumTessFaces(dm);
		mface= dm->getTessFaceArray(dm);

		for(i= 0; i<curnfaces; i++) {
			MFace* mf= &mface[i];

			tri[0]= basenverts + mf->v1;
			tri[1]= basenverts + mf->v3;
			tri[2]= basenverts + mf->v2;
			tri += 3;

			if(mf->v4) {
				tri[0]= basenverts + mf->v1;
				tri[1]= basenverts + mf->v4;
				tri[2]= basenverts + mf->v3;
				tri += 3;
			}
		}

		basenverts+= curnverts;
	}

	/* release derived mesh */
	for(dmlink= dms; dmlink; dmlink= dmlink->next) {
		dm= (DerivedMesh*) dmlink->link;
		dm->release(dm);
	}

	BLI_linklist_free(dms, NULL);

	*nverts_r= nverts;
	*verts_r= verts;
	*ntris_r= ntris;
	*tris_r= tris;
}

static int buildNavMesh(const RecastData *recastParams, int nverts, float *verts, int ntris, int *tris,
								 struct recast_polyMesh **pmesh, struct recast_polyMeshDetail **dmesh)
{
	float bmin[3], bmax[3];
	struct recast_heightfield *solid;
	unsigned char *triflags;
	struct recast_compactHeightfield* chf;
	struct recast_contourSet *cset;
	int width, height, walkableHeight, walkableClimb, walkableRadius;
	int minRegionArea, mergeRegionArea, maxEdgeLen;
	float detailSampleDist, detailSampleMaxError;

	recast_calcBounds(verts, nverts, bmin, bmax);

	/* ** Step 1. Initialize build config ** */
	walkableHeight= (int)ceilf(recastParams->agentheight/ recastParams->cellheight);
	walkableClimb= (int)floorf(recastParams->agentmaxclimb / recastParams->cellheight);
	walkableRadius= (int)ceilf(recastParams->agentradius / recastParams->cellsize);
	minRegionArea= (int)(recastParams->regionminsize * recastParams->regionminsize);
	mergeRegionArea= (int)(recastParams->regionmergesize * recastParams->regionmergesize);
	maxEdgeLen= (int)(recastParams->edgemaxlen/recastParams->cellsize);
	detailSampleDist= recastParams->detailsampledist< 0.9f ? 0 :
			recastParams->cellsize * recastParams->detailsampledist;
	detailSampleMaxError= recastParams->cellheight * recastParams->detailsamplemaxerror;

	/* Set the area where the navigation will be build. */
	recast_calcGridSize(bmin, bmax, recastParams->cellsize, &width, &height);

	/* ** Step 2: Rasterize input polygon soup ** */
	/* Allocate voxel heightfield where we rasterize our input data to */
	solid= recast_newHeightfield();

	if(!recast_createHeightfield(solid, width, height, bmin, bmax, recastParams->cellsize, recastParams->cellheight)) {
		recast_destroyHeightfield(solid);

		return 0;
	}

	/* Allocate array that can hold triangle flags */
	triflags= MEM_callocN(sizeof(unsigned char)*ntris, "buildNavMesh triflags");

	/* Find triangles which are walkable based on their slope and rasterize them */
	recast_markWalkableTriangles(RAD2DEG(recastParams->agentmaxslope), verts, nverts, tris, ntris, triflags);
	recast_rasterizeTriangles(verts, nverts, tris, triflags, ntris, solid);
	MEM_freeN(triflags);

	/* ** Step 3: Filter walkables surfaces ** */
	recast_filterLowHangingWalkableObstacles(walkableClimb, solid);
	recast_filterLedgeSpans(walkableHeight, walkableClimb, solid);
	recast_filterWalkableLowHeightSpans(walkableHeight, solid);

	/* ** Step 4: Partition walkable surface to simple regions ** */

	chf= recast_newCompactHeightfield();
	if(!recast_buildCompactHeightfield(walkableHeight, walkableClimb, solid, chf)) {
		recast_destroyHeightfield(solid);
		recast_destroyCompactHeightfield(chf);

		return 0;
	}

	recast_destroyHeightfield(solid);
	solid = NULL;

	if (!recast_erodeWalkableArea(walkableRadius, chf)) {
		recast_destroyCompactHeightfield(chf);

		return 0;
	}

	/* Prepare for region partitioning, by calculating distance field along the walkable surface */
	if(!recast_buildDistanceField(chf)) {
		recast_destroyCompactHeightfield(chf);

		return 0;
	}

	/* Partition the walkable surface into simple regions without holes */
	if(!recast_buildRegions(chf, 0, minRegionArea, mergeRegionArea)) {
		recast_destroyCompactHeightfield(chf);

		return 0;
	}

	/* ** Step 5: Trace and simplify region contours ** */
	/* Create contours */
	cset= recast_newContourSet();

	if(!recast_buildContours(chf, recastParams->edgemaxerror, maxEdgeLen, cset)) {
		recast_destroyCompactHeightfield(chf);
		recast_destroyContourSet(cset);

		return 0;
	}

	/* ** Step 6: Build polygons mesh from contours ** */
	*pmesh= recast_newPolyMesh();
	if(!recast_buildPolyMesh(cset, recastParams->vertsperpoly, *pmesh)) {
		recast_destroyCompactHeightfield(chf);
		recast_destroyContourSet(cset);
		recast_destroyPolyMesh(*pmesh);

		return 0;
	}


	/* ** Step 7: Create detail mesh which allows to access approximate height on each polygon ** */

	*dmesh= recast_newPolyMeshDetail();
	if(!recast_buildPolyMeshDetail(*pmesh, chf, detailSampleDist, detailSampleMaxError, *dmesh)) {
		recast_destroyCompactHeightfield(chf);
		recast_destroyContourSet(cset);
		recast_destroyPolyMesh(*pmesh);
		recast_destroyPolyMeshDetail(*dmesh);

		return 0;
	}

	recast_destroyCompactHeightfield(chf);
	recast_destroyContourSet(cset);

	return 1;
}

static Object* createRepresentation(bContext *C, struct recast_polyMesh *pmesh, struct recast_polyMeshDetail *dmesh, Base* base)
{
	float co[3], rot[3];
	BMEditMesh *em;
	int i,j, k;
	unsigned short* v;
	int face[3];
	Scene *scene= CTX_data_scene(C);
	Object* obedit;
	int createob= base==NULL;
	int nverts, nmeshes, nvp;
	unsigned short *verts, *polys;
	unsigned int *meshes;
	float bmin[3], cs, ch, *dverts;
	unsigned char *tris;

	zero_v3(co);
	zero_v3(rot);

	if(createob) {
		/* create new object */
		obedit= ED_object_add_type(C, OB_MESH, co, rot, FALSE, 1);
	}
	else {
		obedit= base->object;
		scene_select_base(scene, base);
		copy_v3_v3(obedit->loc, co);
		copy_v3_v3(obedit->rot, rot);
	}

	ED_object_enter_editmode(C, EM_DO_UNDO|EM_IGNORE_LAYER);
	em= (((Mesh *)obedit->data))->edit_btmesh;

	if(!createob) {
		/* clear */
		EDBM_ClearMesh(em);
	}

	/* create verts for polygon mesh */
	verts= recast_polyMeshGetVerts(pmesh, &nverts);
	recast_polyMeshGetBoundbox(pmesh, bmin, NULL);
	recast_polyMeshGetCell(pmesh, &cs, &ch);

	for(i= 0; i<nverts; i++) {
		v= &verts[3*i];
		co[0]= bmin[0] + v[0]*cs;
		co[1]= bmin[1] + v[1]*ch;
		co[2]= bmin[2] + v[2]*cs;
		SWAP(float, co[1], co[2]);
		BM_vert_create(em->bm, co, NULL);
	}

	/* create custom data layer to save polygon idx */
	CustomData_add_layer_named(&em->bm->pdata, CD_RECAST, CD_CALLOC, NULL, 0, "createRepresentation recastData");
	
	/* create verts and faces for detailed mesh */
	meshes= recast_polyMeshDetailGetMeshes(dmesh, &nmeshes);
	polys= recast_polyMeshGetPolys(pmesh, NULL, &nvp);
	dverts= recast_polyMeshDetailGetVerts(dmesh, NULL);
	tris= recast_polyMeshDetailGetTris(dmesh, NULL);

	for(i= 0; i<nmeshes; i++) {
		int uniquevbase= em->bm->totvert;
		unsigned int vbase= meshes[4*i+0];
		unsigned short ndv= meshes[4*i+1];
		unsigned short tribase= meshes[4*i+2];
		unsigned short trinum= meshes[4*i+3];
		const unsigned short* p= &polys[i*nvp*2];
		int nv= 0;

		for(j= 0; j < nvp; ++j) {
			if(p[j]==0xffff) break;
			nv++;
		}

		/* create unique verts  */
		for(j= nv; j<ndv; j++) {
			copy_v3_v3(co, &dverts[3*(vbase + j)]);
			SWAP(float, co[1], co[2]);
			BM_vert_create(em->bm, co, NULL);
		}

		EDBM_init_index_arrays(em, 1, 0, 0);

		/* create faces */
		for(j= 0; j<trinum; j++) {
			unsigned char* tri= &tris[4*(tribase+j)];
			BMFace* newFace;
			int* polygonIdx;

			for(k= 0; k<3; k++) {
				if(tri[k]<nv)
					face[k] = p[tri[k]]; /* shared vertex */
				else
					face[k] = uniquevbase+tri[k]-nv; /* unique vertex */
			}
			newFace= BM_face_create_quad_tri(em->bm,
			                              EDBM_get_vert_for_index(em, face[0]),
			                              EDBM_get_vert_for_index(em, face[2]),
			                              EDBM_get_vert_for_index(em, face[1]), NULL,
			                              NULL, FALSE);

			/* set navigation polygon idx to the custom layer */
			polygonIdx= (int*)CustomData_bmesh_get(&em->bm->pdata, newFace->head.data, CD_RECAST);
			*polygonIdx= i+1; /* add 1 to avoid zero idx */
		}
		
		EDBM_free_index_arrays(em);
	}

	recast_destroyPolyMesh(pmesh);
	recast_destroyPolyMeshDetail(dmesh);

	DAG_id_tag_update((ID*)obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);


	ED_object_exit_editmode(C, EM_FREEDATA); 
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, obedit);

	if(createob) {
		obedit->gameflag&= ~OB_COLLISION;
		obedit->gameflag|= OB_NAVMESH;
		obedit->body_type= OB_BODY_TYPE_NAVMESH;
		rename_id((ID *)obedit, "Navmesh");
	}

	BKE_mesh_ensure_navmesh(obedit->data);

	return obedit;
}

static int create_navmesh_exec(bContext *C, wmOperator *op)
{
	Scene* scene= CTX_data_scene(C);
	LinkNode* obs= NULL;
	Base* navmeshBase= NULL;

	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		if (base->object->type == OB_MESH) {
			if (base->object->body_type==OB_BODY_TYPE_NAVMESH) {
				if (!navmeshBase || base == scene->basact) {
					navmeshBase= base;
				}
			}
			else {
				BLI_linklist_append(&obs, (void*)base->object);
			}
		}
	}
	CTX_DATA_END;

	if (obs) {
		struct recast_polyMesh *pmesh= NULL;
		struct recast_polyMeshDetail *dmesh= NULL;

		int nverts= 0, ntris= 0;
		int *tris= 0;
		float *verts= NULL;

		createVertsTrisData(C, obs, &nverts, &verts, &ntris, &tris);
		BLI_linklist_free(obs, NULL);
		buildNavMesh(&scene->gm.recastData, nverts, verts, ntris, tris, &pmesh, &dmesh);
		createRepresentation(C, pmesh, dmesh, navmeshBase);

		MEM_freeN(verts);
		MEM_freeN(tris);

		return OPERATOR_FINISHED;
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "No mesh objects found");

		return OPERATOR_CANCELLED;
	}
}

void MESH_OT_navmesh_make(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Create navigation mesh";
	ot->description= "Create navigation mesh for selected objects";
	ot->idname= "MESH_OT_navmesh_make";

	/* api callbacks */
	ot->exec= create_navmesh_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int navmesh_face_copy_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;

	/* do work here */
	BMFace *efa_act= BM_active_face_get(em->bm, FALSE);

	if(efa_act) {
		if(CustomData_has_layer(&em->bm->pdata, CD_RECAST)) {
			BMFace *efa;
			BMIter iter;
			int targetPolyIdx= *(int*)CustomData_bmesh_get(&em->bm->pdata, efa_act->head.data, CD_RECAST);
			targetPolyIdx= targetPolyIdx>=0? targetPolyIdx : -targetPolyIdx;

			if(targetPolyIdx > 0) {
				/* set target poly idx to other selected faces */
				BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
					if(BM_elem_flag_test(efa, BM_ELEM_SELECT) && efa != efa_act)  {
						int* recastDataBlock= (int*)CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_RECAST);
						*recastDataBlock= targetPolyIdx;
					}
				}
			}
			else {
				BKE_report(op->reports, RPT_ERROR, "Active face has no index set");
			}
		}
	}

	DAG_id_tag_update((ID*)obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_navmesh_face_copy(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "NavMesh Copy Face Index";
	ot->description= "Copy the index from the active face";
	ot->idname= "MESH_OT_navmesh_face_copy";

	/* api callbacks */
	ot->poll= ED_operator_editmesh;
	ot->exec= navmesh_face_copy_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int compare(const void * a, const void * b)
{
	return ( *(int*)a - *(int*)b );
}

static int findFreeNavPolyIndex(BMEditMesh* em)
{
	/* construct vector of indices */
	int numfaces= em->bm->totface;
	int* indices= MEM_callocN(sizeof(int)*numfaces, "findFreeNavPolyIndex(indices)");
	BMFace* ef;
	BMIter iter;
	int i, idx= em->bm->totface-1, freeIdx= 1;

	/*XXX this originally went last to first, but that isn't possible anymore*/
	BM_ITER(ef, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		int polyIdx= *(int*)CustomData_bmesh_get(&em->bm->pdata, ef->head.data, CD_RECAST);
		indices[idx]= polyIdx;
		idx--;
	}

	qsort(indices, numfaces, sizeof(int), compare);

	/* search first free index */
	freeIdx= 1;
	for(i= 0; i<numfaces; i++) {
		if(indices[i]==freeIdx)
			freeIdx++;
		else if(indices[i]>freeIdx)
			break;
	}

	MEM_freeN(indices);

	return freeIdx;
}

static int navmesh_face_add_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	BMFace *ef;
	BMIter iter;
	
	if(CustomData_has_layer(&em->bm->pdata, CD_RECAST)) {
		int targetPolyIdx= findFreeNavPolyIndex(em);

		if(targetPolyIdx>0) {
			/* set target poly idx to selected faces */
			/*XXX this originally went last to first, but that isn't possible anymore*/
			
			BM_ITER(ef, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
				if(BM_elem_flag_test(ef, BM_ELEM_SELECT)) {
					int *recastDataBlock= (int*)CustomData_bmesh_get(&em->bm->pdata, ef->head.data, CD_RECAST);
					*recastDataBlock= targetPolyIdx;
				}
			}
		}
	}

	DAG_id_tag_update((ID*)obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_navmesh_face_add(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "NavMesh New Face Index";
	ot->description= "Add a new index and assign it to selected faces";
	ot->idname= "MESH_OT_navmesh_face_add";

	/* api callbacks */
	ot->poll= ED_operator_editmesh;
	ot->exec= navmesh_face_add_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int navmesh_obmode_data_poll(bContext *C)
{
	Object *ob = ED_object_active_context(C);
	if (ob && (ob->mode == OB_MODE_OBJECT) && (ob->type == OB_MESH)) {
		Mesh *me= ob->data;
		return CustomData_has_layer(&me->pdata, CD_RECAST);
	}
	return FALSE;
}

static int navmesh_obmode_poll(bContext *C)
{
	Object *ob = ED_object_active_context(C);
	if (ob && (ob->mode == OB_MODE_OBJECT) && (ob->type == OB_MESH)) {
		return TRUE;
	}
	return FALSE;
}

static int navmesh_reset_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_active_context(C);
	Mesh *me= ob->data;

	CustomData_free_layers(&me->pdata, CD_RECAST, me->totpoly);

	BKE_mesh_ensure_navmesh(me);

	DAG_id_tag_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, &me->id);

	return OPERATOR_FINISHED;
}

void MESH_OT_navmesh_reset(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "NavMesh Reset Index Values";
	ot->description= "Assign a new index to every face";
	ot->idname= "MESH_OT_navmesh_reset";

	/* api callbacks */
	ot->poll= navmesh_obmode_poll;
	ot->exec= navmesh_reset_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int navmesh_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_active_context(C);
	Mesh *me= ob->data;

	CustomData_free_layers(&me->pdata, CD_RECAST, me->totpoly);

	DAG_id_tag_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, &me->id);

	return OPERATOR_FINISHED;
}

void MESH_OT_navmesh_clear(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "NavMesh Clear Data";
	ot->description= "Remove navmesh data from this mesh";
	ot->idname= "MESH_OT_navmesh_clear";

	/* api callbacks */
	ot->poll= navmesh_obmode_data_poll;
	ot->exec= navmesh_clear_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
