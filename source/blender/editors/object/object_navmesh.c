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
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2004 by Blender Foundation
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): none yet.
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

#include "BLI_editVert.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_math_vector.h"

#include "ED_object.h"
#include "ED_mesh.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "recast-capi.h"

/*mesh/mesh_intern.h */
extern struct EditVert *addvertlist(EditMesh *em, float *vec, struct EditVert *example);
extern struct EditFace *addfacelist(EditMesh *em, struct EditVert *v1, struct EditVert *v2, struct EditVert *v3, struct EditVert *v4, struct EditFace *example, struct EditFace *exampleEdges);
extern void free_vertlist(EditMesh *em, ListBase *edve);
extern void free_edgelist(EditMesh *em, ListBase *lb);
extern void free_facelist(EditMesh *em, ListBase *lb);

static void createVertsTrisData(bContext *C, LinkNode* obs, int *nverts_r, float **verts_r, int *ntris_r, int **tris_r)
{
	MVert *mvert;
	int nfaces = 0, *tri, i, curnverts, basenverts, curnfaces;
	MFace *mface;
	float co[3], wco[3];
	Object *ob;
	LinkNode *oblink, *dmlink;
	DerivedMesh *dm;
	Scene* scene = CTX_data_scene(C);
	LinkNode* dms = NULL;

	int nverts, ntris, *tris;
	float *verts;

	nverts = 0;
	ntris = 0;
	//calculate number of verts and tris
	for (oblink = obs; oblink; oblink = oblink->next) 
	{
		ob = (Object*) oblink->link;	
		dm = mesh_create_derived_no_virtual(scene, ob, NULL, CD_MASK_MESH);
		BLI_linklist_append(&dms, (void*)dm);

		nverts += dm->getNumVerts(dm);
		nfaces = dm->getNumFaces(dm);
		ntris += nfaces;

		//resolve quad faces
		mface = dm->getFaceArray(dm);
		for (i=0; i<nfaces; i++)
		{
			MFace* mf = &mface[i];
			if (mf->v4)
				ntris+=1;
		}
	}

	//create data
	verts = MEM_mallocN(sizeof(float)*3*nverts, "verts");
	tris = MEM_mallocN(sizeof(int)*3*ntris, "faces");

	basenverts = 0;
	tri = tris;
	for (oblink = obs, dmlink = dms; oblink && dmlink; 
			oblink = oblink->next, dmlink = dmlink->next)
	{
		ob = (Object*) oblink->link;
		dm = (DerivedMesh*) dmlink->link;

		curnverts = dm->getNumVerts(dm);
		mvert = dm->getVertArray(dm);
		//copy verts	
		for (i=0; i<curnverts; i++)
		{
			MVert *v = &mvert[i];
			copy_v3_v3(co, v->co);
			mul_v3_m4v3(wco, ob->obmat, co);
			verts[3*(basenverts+i)+0] = wco[0];
			verts[3*(basenverts+i)+1] = wco[2];
			verts[3*(basenverts+i)+2] = wco[1];
		}

		//create tris
		curnfaces = dm->getNumFaces(dm);
		mface = dm->getFaceArray(dm);
		for (i=0; i<curnfaces; i++)
		{
			MFace* mf = &mface[i]; 
			tri[0]= basenverts + mf->v1; tri[1]= basenverts + mf->v3;	tri[2]= basenverts + mf->v2; 
			tri += 3;
			if (mf->v4)
			{
				tri[0]= basenverts + mf->v1; tri[1]= basenverts + mf->v4; tri[2]= basenverts + mf->v3; 
				tri += 3;
			}
		}
		basenverts += curnverts;
	}

	//release derived mesh
	for (dmlink = dms; dmlink; dmlink = dmlink->next)
	{
		dm = (DerivedMesh*) dmlink->link;
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
	int minRegionSize, mergeRegionSize, maxEdgeLen;
	float detailSampleDist, detailSampleMaxError;

	recast_calcBounds(verts, nverts, bmin, bmax);

	//
	// Step 1. Initialize build config.
	//
	{
/*
		float cellsize = 0.3f;
		float cellheight = 0.2f;
		float agentmaxslope = M_PI/4;
		float agentmaxclimb = 0.9f;
		float agentheight = 2.0f;
		float agentradius = 0.6f;
		float edgemaxlen = 12.0f;
		float edgemaxerror = 1.3f;
		float regionminsize = 50.f;
		float regionmergesize = 20.f;
		int vertsperpoly = 6;
		float detailsampledist = 6.0f;
		float detailsamplemaxerror = 1.0f;
		cfg.cs = cellsize;
		cfg.ch = cellheight;
		cfg.walkableSlopeAngle = agentmaxslope/M_PI*180.f;
		cfg.walkableHeight = (int)ceilf(agentheight/ cfg.ch);
		cfg.walkableClimb = (int)floorf(agentmaxclimb / cfg.ch);
		cfg.walkableRadius = (int)ceilf(agentradius / cfg.cs);
		cfg.maxEdgeLen = (int)(edgemaxlen/cellsize);
		cfg.maxSimplificationError = edgemaxerror;
		cfg.minRegionSize = (int)rcSqr(regionminsize);
		cfg.mergeRegionSize = (int)rcSqr(regionmergesize);
		cfg.maxVertsPerPoly = vertsperpoly;
		cfg.detailSampleDist = detailsampledist< 0.9f ? 0 : cellsize * detailsampledist;
		cfg.detailSampleMaxError = cellheight * detailsamplemaxerror;
*/
#if 0
		cfg.cs = recastParams.cellsize;
		cfg.ch = recastParams.cellheight;
		cfg.walkableSlopeAngle = recastParams.agentmaxslope/((float)M_PI)*180.f;
		cfg.walkableHeight = (int)ceilf(recastParams.agentheight/ cfg.ch);
		cfg.walkableClimb = (int)floorf(recastParams.agentmaxclimb / cfg.ch);
		cfg.walkableRadius = (int)ceilf(recastParams.agentradius / cfg.cs);
		cfg.maxEdgeLen = (int)(recastParams.edgemaxlen/recastParams.cellsize);
		cfg.maxSimplificationError = recastParams.edgemaxerror;
		cfg.minRegionSize = (int)rcSqr(recastParams.regionminsize);
		cfg.mergeRegionSize = (int)rcSqr(recastParams.regionmergesize);
		cfg.maxVertsPerPoly = recastParams.vertsperpoly;
		cfg.detailSampleDist = recastParams.detailsampledist< 0.9f ? 0 : 
								recastParams.cellsize * recastParams.detailsampledist;
		cfg.detailSampleMaxError = recastParams.cellheight * recastParams.detailsamplemaxerror;
#endif
	}

	walkableHeight = (int)ceilf(recastParams->agentheight/ recastParams->cellheight);
	walkableClimb = (int)floorf(recastParams->agentmaxclimb / recastParams->cellheight);
	walkableRadius = (int)ceilf(recastParams->agentradius / recastParams->cellsize);
	minRegionSize = (int)(recastParams->regionminsize * recastParams->regionminsize);
	mergeRegionSize = (int)(recastParams->regionmergesize * recastParams->regionmergesize);
	maxEdgeLen = (int)(recastParams->edgemaxlen/recastParams->cellsize);
	detailSampleDist = recastParams->detailsampledist< 0.9f ? 0 : 
			recastParams->cellsize * recastParams->detailsampledist;
	detailSampleMaxError = recastParams->cellheight * recastParams->detailsamplemaxerror;

	// Set the area where the navigation will be build.
	recast_calcGridSize(bmin, bmax, recastParams->cellsize, &width, &height);

	//
	// Step 2. Rasterize input polygon soup.
	//
	// Allocate voxel heightfield where we rasterize our input data to.
	solid = recast_newHeightfield();

	if (!recast_createHeightfield(solid, width, height, bmin, bmax, recastParams->cellsize, recastParams->cellheight)) {
		recast_destroyHeightfield(solid);

		return 0;
	}

	// Allocate array that can hold triangle flags.
	triflags = MEM_callocN(sizeof(unsigned char)*ntris, "triflags");

	// Find triangles which are walkable based on their slope and rasterize them.
	recast_markWalkableTriangles(RAD2DEG(recastParams->agentmaxslope), verts, nverts, tris, ntris, triflags);
	recast_rasterizeTriangles(verts, nverts, tris, triflags, ntris, solid);
	MEM_freeN(triflags);

	//
	// Step 3. Filter walkables surfaces.
	//
	recast_filterLedgeSpans(walkableHeight, walkableClimb, solid);
	recast_filterWalkableLowHeightSpans(walkableHeight, solid);

	//
	// Step 4. Partition walkable surface to simple regions.
	//

	chf = recast_newCompactHeightfield();
	if (!recast_buildCompactHeightfield(walkableHeight, walkableClimb, RECAST_WALKABLE, solid, chf)) {
		recast_destroyHeightfield(solid);
		recast_destroyCompactHeightfield(chf);

		return 0;
	}

	recast_destroyHeightfield(solid);

	// Prepare for region partitioning, by calculating distance field along the walkable surface.
	if (!recast_buildDistanceField(chf)) {
		recast_destroyCompactHeightfield(chf);

		return 0;
	}

	// Partition the walkable surface into simple regions without holes.
	if (!recast_buildRegions(chf, walkableRadius, 0, minRegionSize, mergeRegionSize)) {
		recast_destroyCompactHeightfield(chf);

		return 0;
	}

	//
	// Step 5. Trace and simplify region contours.
	//
	// Create contours.
	cset = recast_newContourSet();

	if (!recast_buildContours(chf, recastParams->edgemaxerror, maxEdgeLen, cset)) {
		recast_destroyCompactHeightfield(chf);
		recast_destroyContourSet(cset);

		return 0;
	}

	//
	// Step 6. Build polygons mesh from contours.
	//
	*pmesh = recast_newPolyMesh();
	if (!recast_buildPolyMesh(cset, recastParams->vertsperpoly, *pmesh)) {
		recast_destroyCompactHeightfield(chf);
		recast_destroyContourSet(cset);
		recast_destroyPolyMesh(*pmesh);

		return 0;
	}


	//
	// Step 7. Create detail mesh which allows to access approximate height on each polygon.
	//

	*dmesh = recast_newPolyMeshDetail();
	if (!recast_buildPolyMeshDetail(*pmesh, chf, detailSampleDist, detailSampleMaxError, *dmesh)) {
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
	EditMesh *em;
	int i,j, k;
	unsigned short* v;
	int face[3];
	Main *bmain = CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	Object* obedit;
	int createob = base==NULL;
	int nverts, nmeshes, nvp;
	unsigned short *verts, *meshes, *polys;
	float bmin[3], cs, ch, *dverts;
	unsigned char *tris;
	ModifierData *md;

	zero_v3(co);
	zero_v3(rot);

	if (createob)
	{
		//create new object
		obedit = ED_object_add_type(C, OB_MESH, co, rot, FALSE, 1);
	}
	else
	{
		obedit = base->object;
		scene_select_base(scene, base);
		copy_v3_v3(obedit->loc, co);
		copy_v3_v3(obedit->rot, rot);
	}

	ED_object_enter_editmode(C, EM_DO_UNDO|EM_IGNORE_LAYER);
	em = BKE_mesh_get_editmesh(((Mesh *)obedit->data));

	if (!createob)
	{
		//clear
		if(em->verts.first) free_vertlist(em, &em->verts);
		if(em->edges.first) free_edgelist(em, &em->edges);
		if(em->faces.first) free_facelist(em, &em->faces);
		if(em->selected.first) BLI_freelistN(&(em->selected));
	}

	//create verts for polygon mesh
	verts = recast_polyMeshGetVerts(pmesh, &nverts);
	recast_polyMeshGetBoundbox(pmesh, bmin, NULL);
	recast_polyMeshGetCell(pmesh, &cs, &ch);

	for(i = 0; i < nverts; i++) {
		v = &verts[3*i];
		co[0] = bmin[0] + v[0]*cs;
		co[1] = bmin[1] + v[1]*ch;
		co[2] = bmin[2] + v[2]*cs;
		SWAP(float, co[1], co[2]);
		addvertlist(em, co, NULL);
	}

	//create custom data layer to save polygon idx
	CustomData_add_layer_named(&em->fdata, CD_RECAST, CD_CALLOC, NULL, 0, "recastData");

	//create verts and faces for detailed mesh
	meshes = recast_polyMeshDetailGetMeshes(dmesh, &nmeshes);
	polys = recast_polyMeshGetPolys(pmesh, NULL, &nvp);
	dverts = recast_polyMeshDetailGetVerts(dmesh, NULL);
	tris = recast_polyMeshDetailGetTris(dmesh, NULL);

	for (i=0; i<nmeshes; i++)
	{
		int uniquevbase = em->totvert;
		unsigned short vbase = meshes[4*i+0];
		unsigned short ndv = meshes[4*i+1];
		unsigned short tribase = meshes[4*i+2];
		unsigned short trinum = meshes[4*i+3];
		const unsigned short* p = &polys[i*nvp*2];
		int nv = 0;
		for (j = 0; j < nvp; ++j)
		{
			if (p[j] == 0xffff) break;
			nv++;
		}
		//create unique verts 
		for (j=nv; j<ndv; j++)
		{
			copy_v3_v3(co, &dverts[3*(vbase + j)]);
			SWAP(float, co[1], co[2]);
			addvertlist(em, co, NULL);
		}

		EM_init_index_arrays(em, 1, 0, 0);
		
		//create faces
		for (j=0; j<trinum; j++)
		{
			unsigned char* tri = &tris[4*(tribase+j)];
			EditFace* newFace;
			int* polygonIdx;

			for (k=0; k<3; k++)
			{
				if (tri[k]<nv)
					face[k] = p[tri[k]]; //shared vertex
				else
					face[k] = uniquevbase+tri[k]-nv; //unique vertex
			}
			newFace = addfacelist(em, EM_get_vert_for_index(face[0]), EM_get_vert_for_index(face[2]), 
									EM_get_vert_for_index(face[1]), NULL, NULL, NULL);

			//set navigation polygon idx to the custom layer
			polygonIdx = (int*)CustomData_em_get(&em->fdata, newFace->data, CD_RECAST);
			*polygonIdx = i+1; //add 1 to avoid zero idx
		}
		
		EM_free_index_arrays();
	}

	recast_destroyPolyMesh(pmesh);
	recast_destroyPolyMeshDetail(dmesh);

	BKE_mesh_end_editmesh((Mesh*)obedit->data, em);
	
	DAG_id_tag_update((ID*)obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);


	ED_object_exit_editmode(C, EM_FREEDATA); 
	WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, obedit);

	if (createob)
	{
		obedit->gameflag &= ~OB_COLLISION;
		obedit->gameflag |= OB_NAVMESH;
		obedit->body_type = OB_BODY_TYPE_NAVMESH;
		rename_id((ID *)obedit, "Navmesh");
	}
	
	md= modifiers_findByType(obedit, eModifierType_NavMesh);
	if (!md)
	{
		ED_object_modifier_add(NULL, bmain, scene, obedit, NULL, eModifierType_NavMesh);
	}

	return obedit;
}

static int create_navmesh_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene* scene = CTX_data_scene(C);
	int nverts, ntris;
	float* verts;
	int* tris;
	struct recast_polyMesh *pmesh;
	struct recast_polyMeshDetail *dmesh;
	LinkNode* obs = NULL;
	Base* navmeshBase = NULL;

	//CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) //expand macros to avoid error in convertion from void*
	{
		ListBase ctx_data_list;
		CollectionPointerLink *ctx_link;
		CTX_data_selected_editable_bases(C, &ctx_data_list);
		for(ctx_link = (CollectionPointerLink *)ctx_data_list.first; 
				ctx_link; ctx_link = (CollectionPointerLink *)ctx_link->next) {
		Base* base= (Base*)ctx_link->ptr.data;
	{
		if (base->object->body_type==OB_BODY_TYPE_NAVMESH)
		{
			if (!navmeshBase || base==CTX_data_active_base(C))
				navmeshBase = base;
		}
		else
			BLI_linklist_append(&obs, (void*)base->object);
	}
	CTX_DATA_END;
	createVertsTrisData(C, obs, &nverts, &verts, &ntris, &tris);
	BLI_linklist_free(obs, NULL);
	buildNavMesh(&scene->gm.recastData, nverts, verts, ntris, tris, &pmesh, &dmesh);
	createRepresentation(C, pmesh, dmesh, navmeshBase);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_create_navmesh(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Create navigation mesh";
	ot->description= "Create navigation mesh for selected objects";
	ot->idname= "OBJECT_OT_create_navmesh";

	/* api callbacks */
	ot->exec= create_navmesh_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int assign_navpolygon_poll(bContext *C)
{
	Object *ob= (Object *)CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	if (!ob || !ob->data)
		return 0;
	return (((Mesh*)ob->data)->edit_mesh != NULL);
}

static int assign_navpolygon_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh *)obedit->data);

	//do work here
	int targetPolyIdx = -1;
	EditFace *ef, *efa;
	efa = EM_get_actFace(em, 0);
	if (efa) 
	{
		if (CustomData_has_layer(&em->fdata, CD_RECAST))
		{
			targetPolyIdx = *(int*)CustomData_em_get(&em->fdata, efa->data, CD_RECAST);
			targetPolyIdx = targetPolyIdx>=0? targetPolyIdx : -targetPolyIdx;
			if (targetPolyIdx>0)
			{
				//set target poly idx to other selected faces
				ef = (EditFace*)em->faces.last;
				while(ef) 
				{
					if((ef->f & SELECT )&& ef!=efa) 
					{
						int* recastDataBlock = (int*)CustomData_em_get(&em->fdata, ef->data, CD_RECAST);
						*recastDataBlock = targetPolyIdx;
					}
					ef = ef->prev;
				}
			}
		}		
	}
	
	DAG_id_tag_update((ID*)obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	BKE_mesh_end_editmesh((Mesh*)obedit->data, em);
	return OPERATOR_FINISHED;
}

void OBJECT_OT_assign_navpolygon(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Assign polygon index";
	ot->description= "Assign polygon index to face by active face";
	ot->idname= "OBJECT_OT_assign_navpolygon";

	/* api callbacks */
	ot->poll = assign_navpolygon_poll;
	ot->exec= assign_navpolygon_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int compare(const void * a, const void * b){  
	return ( *(int*)a - *(int*)b );
}

static int findFreeNavPolyIndex(EditMesh* em)
{
	//construct vector of indices
	int numfaces = em->totface;
	int* indices = MEM_callocN(sizeof(int)*numfaces, "findFreeNavPolyIndex(indices)");
	EditFace* ef = (EditFace*)em->faces.last;
	int i, idx = 0, freeIdx = 1;

	while(ef) 
	{
		int polyIdx = *(int*)CustomData_em_get(&em->fdata, ef->data, CD_RECAST);
		indices[idx] = polyIdx;
		idx++;
		ef = ef->prev;
	}

	qsort(indices, numfaces, sizeof(int), compare);

	//search first free index
	freeIdx = 1;
	for (i=0; i<numfaces; i++)
	{
		if (indices[i]==freeIdx)
			freeIdx++;
		else if (indices[i]>freeIdx)
			break;
	}

	MEM_freeN(indices);

	return freeIdx;
}

static int assign_new_navpolygon_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh((Mesh *)obedit->data);

	EditFace *ef;
	if (CustomData_has_layer(&em->fdata, CD_RECAST))
	{
		int targetPolyIdx = findFreeNavPolyIndex(em);
		if (targetPolyIdx>0)
		{
			//set target poly idx to selected faces
			ef = (EditFace*)em->faces.last;
			while(ef) 
			{
				if(ef->f & SELECT ) 
				{
					int* recastDataBlock = (int*)CustomData_em_get(&em->fdata, ef->data, CD_RECAST);
					*recastDataBlock = targetPolyIdx;
				}
				ef = ef->prev;
			}
		}
	}		

	DAG_id_tag_update((ID*)obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	BKE_mesh_end_editmesh((Mesh*)obedit->data, em);
	return OPERATOR_FINISHED;
}

void OBJECT_OT_assign_new_navpolygon(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Assign new polygon index";
	ot->description= "Assign new polygon index to face";
	ot->idname= "OBJECT_OT_assign_new_navpolygon";

	/* api callbacks */
	ot->poll = assign_navpolygon_poll;
	ot->exec= assign_new_navpolygon_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
