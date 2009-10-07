/**
 * $Id: editderivedbmesh.c 18571 2009-01-19 06:04:57Z joeedh $
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
 * Inc., 59 Tbmple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#include "BLI_editVert.h"
#include "BLI_edgehash.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"
#include "BLI_scanfill.h"
#include "BLI_ghash.h"
#include "BLI_array.h"

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
#include "BKE_subsurf.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_particle.h"
#include "BKE_tessmesh.h"

#include "BLO_sys_types.h" // for intptr_t support

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_material.h"

#include "bmesh.h"

BMEditMesh *BMEdit_Create(BMesh *bm)
{
	BMEditMesh *tm = MEM_callocN(sizeof(BMEditMesh), "tm");
	
	tm->bm = bm;

	BMEdit_RecalcTesselation(tm);

	return tm;
}

BMEditMesh *BMEdit_Copy(BMEditMesh *tm)
{
	BMEditMesh *tm2 = MEM_callocN(sizeof(BMEditMesh), "tm2");
	*tm2 = *tm;
	
	tm2->derivedCage = tm2->derivedFinal = NULL;
	
	tm2->looptris = NULL;
	tm2->bm = BM_Copy_Mesh(tm->bm);
	BMEdit_RecalcTesselation(tm2);

	tm2->vert_index = NULL;
	tm2->edge_index = NULL;
	tm2->face_index = NULL;

	return tm2;
}

static void BMEdit_RecalcTesselation_intern(BMEditMesh *tm)
{
	BMesh *bm = tm->bm;
	BMLoop **looptris = NULL;
	BLI_array_declare(looptris);
	BMIter iter, liter;
	BMFace *f;
	BMLoop *l;
	int i = 0, j, a, b;
	
	if (tm->looptris) MEM_freeN(tm->looptris);

#if 0 //simple quad/triangle code for performance testing purposes
	looptris = MEM_callocN(sizeof(void*)*bm->totface*8, "looptris");

	f = BMIter_New(&iter, bm, BM_FACES_OF_MESH, NULL);
	for ( ; f; f=BMIter_Step(&iter)) {
		EditVert *v, *lastv=NULL, *firstv=NULL;
		EditEdge *e;
		EditFace *efa;

		/*don't consider two-edged faces*/
		if (f->len < 3) continue;
		
		//BLI_array_growone(looptris);
		//BLI_array_growone(looptris);
		//BLI_array_growone(looptris);

		looptris[i*3] = f->loopbase;
		looptris[i*3+1] = f->loopbase->head.next;
		looptris[i*3+2] = f->loopbase->head.next->next;
		i++;

		if (f->len > 3) {
			//BLI_array_growone(looptris);
			//BLI_array_growone(looptris);
			//BLI_array_growone(looptris);

			looptris[i*3] = f->loopbase;
			looptris[i*3+1] = f->loopbase->head.next->next;
			looptris[i*3+2] = f->loopbase->head.next->next->next;
			i++;
		}

	}

	tm->tottri = i;
	tm->looptris = looptris;
	return;
#endif

	f = BMIter_New(&iter, bm, BM_FACES_OF_MESH, NULL);
	for ( ; f; f=BMIter_Step(&iter)) {
		EditVert *v, *lastv=NULL, *firstv=NULL;
		EditEdge *e;
		EditFace *efa;

		/*don't consider two-edged faces*/
		if (f->len < 3) continue;
		
		/*scanfill time*/
		l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
		for (j=0; l; l=BMIter_Step(&liter), j++) {
			/*mark order*/
			l->head.eflag2 = j;

			v = BLI_addfillvert(l->v->co);
			v->tmp.p = l;
			
			if (lastv) {
				e = BLI_addfilledge(lastv, v);
			}

			lastv = v;
			if (firstv==NULL) firstv = v;
		}

		/*complete the loop*/
		BLI_addfilledge(firstv, v);

		BLI_edgefill(0, 0);
		
		for (efa = fillfacebase.first; efa; efa=efa->next) {
			BMLoop *l1, *l2, *l3;

			BLI_array_growone(looptris);
			BLI_array_growone(looptris);
			BLI_array_growone(looptris);
			
			looptris[i*3] = l1 = efa->v1->tmp.p;
			looptris[i*3+1] = l2 = efa->v2->tmp.p;
			looptris[i*3+2] = l3 = efa->v3->tmp.p;
			
			if (l1->head.eflag2 > l2->head.eflag2) {
				SWAP(BMLoop*, l1, l2);
			}
			if (l2->head.eflag2 > l3->head.eflag2) {
				SWAP(BMLoop*, l2, l3);
			}
			if (l1->head.eflag2 > l2->head.eflag2) {
				SWAP(BMLoop*, l1, l2);
			}
			
			looptris[i*3] = l1;
			looptris[i*3+1] = l2;
			looptris[i*3+2] = l3;

			i += 1;
		}

		BLI_end_edgefill();
	}

	tm->tottri = i;
	tm->looptris = looptris;
}

void BMEdit_RecalcTesselation(BMEditMesh *tm)
{
	BMEdit_RecalcTesselation_intern(tm);

	if (tm->derivedFinal && tm->derivedFinal == tm->derivedCage) {
		if (tm->derivedFinal->recalcTesselation) 
			tm->derivedFinal->recalcTesselation(tm->derivedFinal);
	} else if (tm->derivedFinal) {
		if (tm->derivedCage->recalcTesselation) 
			tm->derivedCage->recalcTesselation(tm->derivedCage);
		if (tm->derivedFinal->recalcTesselation) 
			tm->derivedFinal->recalcTesselation(tm->derivedFinal);
	}
}

void BMEdit_UpdateLinkedCustomData(BMEditMesh *em)
{
	BMesh *bm = em->bm;
	int act;

	if (CustomData_has_layer(&bm->pdata, CD_MTEXPOLY)) {
		act = CustomData_get_active_layer(&bm->pdata, CD_MTEXPOLY);
		CustomData_set_layer_active(&bm->ldata, CD_MLOOPUV, act);

		act = CustomData_get_render_layer(&bm->pdata, CD_MTEXPOLY);
		CustomData_set_layer_render(&bm->ldata, CD_MLOOPUV, act);

		act = CustomData_get_clone_layer(&bm->pdata, CD_MTEXPOLY);
		CustomData_set_layer_clone(&bm->ldata, CD_MLOOPUV, act);

		act = CustomData_get_mask_layer(&bm->pdata, CD_MTEXPOLY);
		CustomData_set_layer_mask(&bm->ldata, CD_MLOOPUV, act);
	}
}

/*does not free the BMEditMesh struct itself*/
void BMEdit_Free(BMEditMesh *em)
{
	if(em->derivedFinal) {
		if (em->derivedFinal!=em->derivedCage) {
			em->derivedFinal->needsFree= 1;
			em->derivedFinal->release(em->derivedFinal);
		}
		em->derivedFinal= NULL;
	}
	if(em->derivedCage) {
		em->derivedCage->needsFree= 1;
		em->derivedCage->release(em->derivedCage);
		em->derivedCage= NULL;
	}

	em->retopo_paint_data= NULL;

	if (em->looptris) MEM_freeN(em->looptris);

	if (em->vert_index) MEM_freeN(em->vert_index);
	if (em->edge_index) MEM_freeN(em->edge_index);
	if (em->face_index) MEM_freeN(em->face_index);

	BM_Free_Mesh(em->bm);
}


/*
ok, basic design:

the bmesh derivedmesh exposes the mesh as triangles.  it stores pointers
to three loops per triangle.  the derivedmesh stores a cache of tesselations
for each face.  this cache will smartly update as needed (though at first
it'll simply be more brute force).  keeping track of face/edge counts may
be a small problbm.

this won't be the most efficient thing, considering that internal edges and
faces of tesselations are exposed.  looking up an edge by index in particular
is likely to be a little slow.
*/

typedef struct EditDerivedBMesh {
	DerivedMesh dm;

	Object *ob;
	BMEditMesh *tc;

	float (*vertexCos)[3];
	float (*vertexNos)[3];
	float (*faceNos)[3];

	/*lookup caches; these are rebuilt on dm->RecalcTesselation()
	  (or when the derivedmesh is created, of course)*/
	GHash *vhash, *ehash, *fhash;
	BMVert **vtable;
	BMEdge **etable;
	BMFace **ftable;

	/*private variables, for number of verts/edges/faces
	  within the above hash/table members*/
	int tv, te, tf;

	/*customdata layout of the tesselated faces*/
	CustomData tessface_layout;
} EditDerivedBMesh;

static void bmdm_recalc_lookups(EditDerivedBMesh *bmdm)
{
	BMIter iter;
	BMHeader *h;
	int a, i, iters[3] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH};
	
	bmdm->tv = bmdm->tc->bm->totvert;
	bmdm->te = bmdm->tc->bm->totedge;
	bmdm->tf = bmdm->tc->bm->totface;

	if (bmdm->vhash) BLI_ghash_free(bmdm->vhash, NULL, NULL);
	if (bmdm->ehash) BLI_ghash_free(bmdm->ehash, NULL, NULL);
	if (bmdm->fhash) BLI_ghash_free(bmdm->fhash, NULL, NULL);

	bmdm->vhash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	bmdm->ehash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	bmdm->fhash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	
	if (bmdm->vtable) MEM_freeN(bmdm->vtable);
	if (bmdm->etable) MEM_freeN(bmdm->etable);
	if (bmdm->ftable) MEM_freeN(bmdm->ftable);
	
	if (bmdm->tc->bm->totvert)
		bmdm->vtable = MEM_mallocN(sizeof(void**)*bmdm->tc->bm->totvert, "bmdm->vtable");
	else bmdm->vtable = NULL;

	if (bmdm->tc->bm->totedge)
		bmdm->etable = MEM_mallocN(sizeof(void**)*bmdm->tc->bm->totedge, "bmdm->etable");
	else bmdm->etable = NULL;
	
	if (bmdm->tc->bm->totface)
		bmdm->ftable = MEM_mallocN(sizeof(void**)*bmdm->tc->bm->totface, "bmdm->ftable");
	else bmdm->ftable = NULL;
	
	for (a=0; a<3; a++) {
		h = BMIter_New(&iter, bmdm->tc->bm, iters[a], NULL);
		for (i=0; h; h=BMIter_Step(&iter), i++) {
			switch (a) {
				case 0:
					bmdm->vtable[i] = (BMVert*) h;
					BLI_ghash_insert(bmdm->vhash, h, SET_INT_IN_POINTER(i));
					break;
				case 1:
					bmdm->etable[i] = (BMEdge*) h;
					BLI_ghash_insert(bmdm->ehash, h, SET_INT_IN_POINTER(i));
					break;
				case 2:
					bmdm->ftable[i] = (BMFace*) h;
					BLI_ghash_insert(bmdm->fhash, h, SET_INT_IN_POINTER(i));
					break;

			}
		}
	}
}

static void bmDM_recalcTesselation(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;

	//bmdm_recalc_lookups(bmdm);
}

static void bmDM_foreachMappedVert(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no_f, short *no_s), void *userData)
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;
	BMVert *eve;
	BMIter iter;
	int i;
	
	eve = BMIter_New(&iter, bmdm->tc->bm, BM_VERTS_OF_MESH, NULL);
	for (i=0; eve; i++, eve=BMIter_Step(&iter)) {
		if (bmdm->vertexCos) {
			func(userData, i, bmdm->vertexCos[i], bmdm->vertexNos[i], NULL);
		} else {
			func(userData, i, eve->co, eve->no, NULL);
		}
	}
}
static void bmDM_foreachMappedEdge(DerivedMesh *dm, void (*func)(void *userData, int index, float *v0co, float *v1co), void *userData)
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;
	BMEdge *eed;
	BMIter iter;
	int i;
	
	if (bmdm->vertexCos) {
		BMVert *eve;
		BMIter viter;

		eve = BMIter_New(&viter, bmdm->tc->bm, BM_VERTS_OF_MESH, NULL);
		for (i=0; eve; eve=BMIter_Step(&viter), i++) {
			BMINDEX_SET(eve, i);
		}

		eed = BMIter_New(&iter, bmdm->tc->bm, BM_EDGES_OF_MESH, NULL);
		for(i=0; eed; i++,eed=BMIter_Step(&iter))
			func(userData, i, 
			     bmdm->vertexCos[BMINDEX_GET(eve)], 
			     bmdm->vertexCos[BMINDEX_GET(eve)]);
	} else {
		eed = BMIter_New(&iter, bmdm->tc->bm, BM_EDGES_OF_MESH, NULL);
		for(i=0; eed; i++,eed=BMIter_Step(&iter))
			func(userData, i, eed->v1->co, eed->v2->co);
	}

}

static void bmDM_drawMappedEdges(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData) 
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;
	BMEdge *eed;
	BMIter iter;
	int i;
	
	if (bmdm->vertexCos) {
		BMVert *eve;
		BMIter viter;

		eve = BMIter_New(&viter, bmdm->tc->bm, BM_VERTS_OF_MESH, NULL);
		for (i=0; eve; eve=BMIter_Step(&viter), i++) {
			BMINDEX_SET(eve, i);
		}

		glBegin(GL_LINES);
		eed = BMIter_New(&iter, bmdm->tc->bm, BM_EDGES_OF_MESH, NULL);
		for(i=0; eed; i++,eed=BMIter_Step(&iter)) {
			if(!setDrawOptions || setDrawOptions(userData, i)) {
				glVertex3fv(bmdm->vertexCos[BMINDEX_GET(eed->v1)]);
				glVertex3fv(bmdm->vertexCos[BMINDEX_GET(eed->v2)]);
			}
		}
		glEnd();

	} else {
		glBegin(GL_LINES);
		eed = BMIter_New(&iter, bmdm->tc->bm, BM_EDGES_OF_MESH, NULL);
		for(i=0; eed; i++,eed=BMIter_Step(&iter)) {
			if(!setDrawOptions || setDrawOptions(userData, i)) {
				glVertex3fv(eed->v1->co);
				glVertex3fv(eed->v2->co);
			}
		}
		glEnd();
	}
}

static void bmDM_drawEdges(DerivedMesh *dm, int drawLooseEdges)
{
	bmDM_drawMappedEdges(dm, NULL, NULL);
}

static void bmDM_drawMappedEdgesInterp(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void (*setDrawInterpOptions)(void *userData, int index, float t), void *userData) 
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;
	BMEdge *eed;
	BMIter iter;
	int i;

	if (bmdm->vertexCos) {
		BMVert *eve;

		eve = BMIter_New(&iter, bmdm->tc->bm, BM_VERTS_OF_MESH, NULL);
		for (i=0; eve; eve=BMIter_Step(&iter), i++)
			BMINDEX_SET(eve, i);

		glBegin(GL_LINES);
		eed = BMIter_New(&iter, bmdm->tc->bm, BM_EDGES_OF_MESH, NULL);
		for(i=0; eed; i++,eed=BMIter_Step(&iter)) {
			if(!setDrawOptions || setDrawOptions(userData, i)) {
				setDrawInterpOptions(userData, i, 0.0);
				glVertex3fv(bmdm->vertexCos[(int) BMINDEX_GET(eed->v1)]);
				setDrawInterpOptions(userData, i, 1.0);
				glVertex3fv(bmdm->vertexCos[(int) BMINDEX_GET(eed->v2)]);
			}
		}
		glEnd();
	} else {
		glBegin(GL_LINES);
		eed = BMIter_New(&iter, bmdm->tc->bm, BM_EDGES_OF_MESH, NULL);
		for(i=0; eed; i++,eed=BMIter_Step(&iter)) {
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

static void bmDM_drawUVEdges(DerivedMesh *dm)
{
#if 0
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;
	BMFace *efa;
	MTFace *tf;

	glBegin(GL_LINES);
	for(efa= bmdm->tc->bm->faces.first; efa; efa= efa->next) {
		tf = CustomData_bm_get(&bmdm->tc->bm->pdata, efa->data, CD_MTFACE);

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
#endif
}

static void bmDM__calcFaceCent(BMesh *bm, BMFace *efa, float cent[3],
                               float (*vertexCos)[3])
{
	BMIter iter;
	BMLoop *l;
	int tot = 0;
	
	cent[0] = cent[1] = cent[2] = 0.0f;
	
	/*simple (and stupid) median (average) based method :/ */
	
	if (vertexCos) {
		l = BMIter_New(&iter, bm, BM_LOOPS_OF_FACE, efa);
		for (; l; l=BMIter_Step(&iter)) {
			VECADD(cent, cent, vertexCos[BMINDEX_GET(l->v)]);
			tot++;
		}
	} else {
		l = BMIter_New(&iter, bm, BM_LOOPS_OF_FACE, efa);
		for (; l; l=BMIter_Step(&iter)) {
			VECADD(cent, cent, l->v->co);
			tot++;
		}
	}

	if (tot==0) return;
	VECMUL(cent, 1.0f/(float)tot);
}

static void bmDM_foreachMappedFaceCenter(DerivedMesh *dm, void (*func)(void *userData, int index, float *co, float *no), void *userData)
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;
	BMVert *eve;
	BMFace *efa;
	BMIter iter;
	float cent[3];
	int i;

	if (bmdm->vertexCos) {
		eve = BMIter_New(&iter, bmdm->tc->bm, BM_VERTS_OF_MESH, NULL);
		for (i=0; eve; eve=BMIter_Step(&iter), i++)
			BMINDEX_SET(eve, i);
	}

	efa = BMIter_New(&iter, bmdm->tc->bm, BM_FACES_OF_MESH, NULL);
	for (i=0; efa; efa=BMIter_Step(&iter), i++) {
		bmDM__calcFaceCent(bmdm->tc->bm, efa, cent, bmdm->vertexCos);
		func(userData, i, cent, bmdm->vertexCos?bmdm->faceNos[i]:efa->no);
	}
}

static void bmDM_drawMappedFaces(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index, int *drawSmooth_r), void *userData, int useColors)
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;
	BMFace *efa;
	BMIter iter;
	int i, draw;

	if (bmdm->vertexCos) {
		BMVert *eve;
		
		eve = BMIter_New(&iter, bmdm->tc->bm, BM_VERTS_OF_MESH, NULL);
		for (i=0; eve; eve=BMIter_Step(&iter), i++)
			BMINDEX_SET(eve, i);

		efa = BMIter_New(&iter, bmdm->tc->bm, BM_FACES_OF_MESH, NULL);
		for (i=0; efa; efa=BMIter_Step(&iter), i++)
			BMINDEX_SET(efa, i);

		for (i=0; i<bmdm->tc->tottri; i++) {
			BMLoop **l = bmdm->tc->looptris[i];
			int drawSmooth;
			
			efa = l[0]->f;
			drawSmooth = (efa->head.flag & BM_SMOOTH);

			draw = setDrawOptions==NULL ? 1 : setDrawOptions(userData, BMINDEX_GET(efa), &drawSmooth);
			if(draw) {
				if (draw==2) { /* enabled with stipple */
		  			glEnable(GL_POLYGON_STIPPLE);
		  			glPolygonStipple(stipple_quarttone);
				}
				
				glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);

				glBegin(GL_TRIANGLES);

				if (!drawSmooth) {
					glNormal3fv(bmdm->faceNos[i]);
					glVertex3fv(bmdm->vertexCos[(int) BMINDEX_GET(l[0]->v)]);
					glVertex3fv(bmdm->vertexCos[(int) BMINDEX_GET(l[1]->v)]);
					glVertex3fv(bmdm->vertexCos[(int) BMINDEX_GET(l[2]->v)]);
				} else {
					glNormal3fv(bmdm->vertexNos[(int) BMINDEX_GET(l[0]->v)]);
					glVertex3fv(bmdm->vertexCos[(int) BMINDEX_GET(l[0]->v)]);
					glNormal3fv(bmdm->vertexNos[(int) BMINDEX_GET(l[1]->v)]);
					glVertex3fv(bmdm->vertexCos[(int) BMINDEX_GET(l[1]->v)]);
					glNormal3fv(bmdm->vertexNos[(int) BMINDEX_GET(l[2]->v)]);
					glVertex3fv(bmdm->vertexCos[(int) BMINDEX_GET(l[2]->v)]);
				}
				glEnd();

				if (draw==2)
					glDisable(GL_POLYGON_STIPPLE);
			}
		}
	} else {
		efa = BMIter_New(&iter, bmdm->tc->bm, BM_FACES_OF_MESH, NULL);
		for (i=0; efa; efa=BMIter_Step(&iter), i++)
			BMINDEX_SET(efa, i);

		for (i=0; i<bmdm->tc->tottri; i++) {
			BMLoop **l = bmdm->tc->looptris[i];
			int drawSmooth;

			efa = l[0]->f;
			drawSmooth = (efa->head.flag & BM_SMOOTH);
			
			draw = setDrawOptions==NULL ? 1 : setDrawOptions(userData, BMINDEX_GET(efa), &drawSmooth);
			if(draw) {
				if (draw==2) { /* enabled with stipple */
		  			glEnable(GL_POLYGON_STIPPLE);
		  			glPolygonStipple(stipple_quarttone);
				}
				glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);
				
				glBegin(GL_TRIANGLES);
				if (!drawSmooth) {
					glNormal3fv(efa->no);
					glVertex3fv(l[0]->v->co);
					glVertex3fv(l[1]->v->co);
					glVertex3fv(l[2]->v->co);
				} else {
					glNormal3fv(l[0]->v->no);
					glVertex3fv(l[0]->v->co);
					glNormal3fv(l[1]->v->no);
					glVertex3fv(l[1]->v->co);
					glNormal3fv(l[2]->v->no);
					glVertex3fv(l[2]->v->co);
				}
				glEnd();
				
				if (draw==2)
					glDisable(GL_POLYGON_STIPPLE);
			}
		}
	}
}

static void bmdm_get_tri_tex(BMesh *bm, BMLoop **ls, MLoopUV *luv[3], MLoopCol *lcol[3], 
			     int has_uv, int has_col)
{
	if (has_uv) { 
		luv[0] = CustomData_bmesh_get(&bm->ldata, ls[0]->head.data, CD_MLOOPUV);
		luv[1] = CustomData_bmesh_get(&bm->ldata, ls[1]->head.data, CD_MLOOPUV);
		luv[2] = CustomData_bmesh_get(&bm->ldata, ls[2]->head.data, CD_MLOOPUV);
	}

	if (has_col) {
		lcol[0] = CustomData_bmesh_get(&bm->ldata, ls[0]->head.data, CD_MLOOPCOL);
		lcol[1] = CustomData_bmesh_get(&bm->ldata, ls[1]->head.data, CD_MLOOPCOL);
		lcol[2] = CustomData_bmesh_get(&bm->ldata, ls[2]->head.data, CD_MLOOPCOL);
	}


}

static void bmDM_drawFacesTex_common(DerivedMesh *dm,
               int (*drawParams)(MTFace *tface, int has_vcol, int matnr),
               int (*drawParamsMapped)(void *userData, int index),
               void *userData) 
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;
	BMEditMesh *em = bmdm->tc;
	BMesh *bm= bmdm->tc->bm;
	float (*vertexCos)[3]= bmdm->vertexCos;
	float (*vertexNos)[3]= bmdm->vertexNos;
	BMFace *efa;
	BMVert *eve;
	BMIter iter;
	MLoopUV *luv[3], dummyluv = {0};
	MLoopCol *lcol[3], dummylcol = {0};
	int i, has_vcol = CustomData_has_layer(&bm->ldata, CD_MLOOPCOL);
	int has_uv = CustomData_has_layer(&bm->pdata, CD_MTEXPOLY);
	
	luv[0] = luv[1] = luv[2] = &dummyluv;
	lcol[0] = lcol[1] = lcol[2] = &dummylcol;

	dummylcol.a = dummylcol.r = dummylcol.g = dummylcol.b = 255;

	/* always use smooth shading even for flat faces, else vertex colors wont interpolate */
	glShadeModel(GL_SMOOTH);
	
	i = 0;
	BM_ITER(efa, &iter, bm, BM_FACES_OF_MESH, NULL)
		BMINDEX_SET(efa, i++);

	if (vertexCos) {
		i = 0;
		BM_ITER(eve, &iter, bm, BM_VERTS_OF_MESH, NULL)
			BMINDEX_SET(eve, i++);
				
		glBegin(GL_TRIANGLES);
		for (i=0; i<em->tottri; i++) {
			BMLoop **ls = em->looptris[i];
			MTexPoly *tp= CustomData_bmesh_get(&bm->pdata, ls[0]->f->head.data, CD_MTEXPOLY);
			MTFace mtf = {0};
			unsigned char *cp= NULL;
			int drawSmooth= BM_TestHFlag(ls[0]->f, BM_SMOOTH);
			int flag;

			efa = ls[0]->f;
			
			if (has_uv) {
				mtf.flag = tp->flag;
				mtf.tpage = tp->tpage;
				mtf.transp = tp->transp;
				mtf.mode = tp->mode;
				mtf.tile = tp->tile;
				mtf.unwrap = tp->unwrap;
			}

			if(drawParams)
				flag= drawParams(&mtf, has_vcol, efa->mat_nr);
			else if(drawParamsMapped)
				flag= drawParamsMapped(userData, BMINDEX_GET(efa));
			else
				flag= 1;

			if(flag != 0) { /* flag 0 == the face is hidden or invisible */
				
				/* we always want smooth here since otherwise vertex colors dont interpolate */
				if (!has_vcol) {
					glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);
				} 
				
				if (!drawSmooth) {
					glNormal3fv(bmdm->faceNos[i]);
					
					bmdm_get_tri_tex(bm, ls, luv, lcol, has_uv, has_vcol);
					
					glTexCoord2fv(luv[0]->uv);
					glColor3ub(lcol[0]->r, lcol[0]->g, lcol[0]->b);
					glVertex3fv(vertexCos[BMINDEX_GET(ls[0]->v)]);

					glTexCoord2fv(luv[1]->uv);
					glColor3ub(lcol[1]->r, lcol[1]->g, lcol[1]->b);
					glVertex3fv(vertexCos[BMINDEX_GET(ls[1]->v)]);

					glTexCoord2fv(luv[2]->uv);
					glColor3ub(lcol[2]->r, lcol[2]->g, lcol[2]->b);
					glVertex3fv(vertexCos[BMINDEX_GET(ls[2]->v)]);
				} else {
					bmdm_get_tri_tex(bm, ls, luv, lcol, has_uv, has_vcol);
					
					glTexCoord2fv(luv[0]->uv);
					glColor3ub(lcol[0]->r, lcol[0]->g, lcol[0]->b);
					glNormal3fv(vertexNos[BMINDEX_GET(ls[0]->v)]);
					glVertex3fv(vertexCos[BMINDEX_GET(ls[0]->v)]);

					glTexCoord2fv(luv[1]->uv);
					glColor3ub(lcol[1]->r, lcol[1]->g, lcol[1]->b);
					glNormal3fv(vertexNos[BMINDEX_GET(ls[1]->v)]);
					glVertex3fv(vertexCos[BMINDEX_GET(ls[1]->v)]);

					glTexCoord2fv(luv[2]->uv);
					glColor3ub(lcol[2]->r, lcol[2]->g, lcol[2]->b);
					glNormal3fv(vertexNos[BMINDEX_GET(ls[2]->v)]);
					glVertex3fv(vertexCos[BMINDEX_GET(ls[2]->v)]);
				}
			}
		}
		glEnd();
	} else {
		i = 0;
		BM_ITER(eve, &iter, bm, BM_VERTS_OF_MESH, NULL)
			BMINDEX_SET(eve, i++);
				
		for (i=0; i<em->tottri; i++) {
			BMLoop **ls = em->looptris[i];
			MTexPoly *tp= CustomData_bmesh_get(&bm->pdata, ls[0]->f->head.data, CD_MTEXPOLY);
			MTFace mtf = {0};
			unsigned char *cp= NULL;
			int drawSmooth= BM_TestHFlag(ls[0]->f, BM_SMOOTH);
			int flag;

			efa = ls[0]->f;
			
			if (has_uv) {
				mtf.flag = tp->flag;
				mtf.tpage = tp->tpage;
				mtf.transp = tp->transp;
				mtf.mode = tp->mode;
				mtf.tile = tp->tile;
				mtf.unwrap = tp->unwrap;
			}

			if(drawParams)
				flag= drawParams(&mtf, has_vcol, efa->mat_nr);
			else if(drawParamsMapped)
				flag= drawParamsMapped(userData, BMINDEX_GET(efa));
			else
				flag= 1;

			if(flag != 0) { /* flag 0 == the face is hidden or invisible */
				
				/* we always want smooth here since otherwise vertex colors dont interpolate */
				if (!has_vcol) {
					glShadeModel(drawSmooth?GL_SMOOTH:GL_FLAT);
				} 
				
				glBegin(GL_TRIANGLES);
				if (!drawSmooth) {
					glNormal3fv(efa->no);
					
					bmdm_get_tri_tex(bm, ls, luv, lcol, has_uv, has_vcol);
					
					if (luv[0])
						glTexCoord2fv(luv[0]->uv);
					if (lcol[0])
						glColor3ub(lcol[0]->r, lcol[0]->g, lcol[0]->b);
					else glColor3ub(0, 0, 0);
					glVertex3fv(ls[0]->v->co);

					if (luv[1])
						glTexCoord2fv(luv[1]->uv);
					if (lcol[1])
						glColor3ub(lcol[1]->r, lcol[1]->g, lcol[1]->b);
					else glColor3ub(0, 0, 0);
					glVertex3fv(ls[1]->v->co);

					if (luv[2])
						glTexCoord2fv(luv[2]->uv);
					if (lcol[2])
						glColor3ub(lcol[2]->r, lcol[2]->g, lcol[2]->b);
					else glColor3ub(0, 0, 0);
					glVertex3fv(ls[2]->v->co);
				} else {
					bmdm_get_tri_tex(bm, ls, luv, lcol, has_uv, has_vcol);
					
					if (luv[0])
						glTexCoord2fv(luv[0]->uv);
					if (lcol[0])
						glColor3ub(lcol[0]->r, lcol[0]->g, lcol[0]->b);
					else glColor3ub(0, 0, 0);
					glNormal3fv(ls[0]->v->no);
					glVertex3fv(ls[0]->v->co);

					if (luv[1])
						glTexCoord2fv(luv[1]->uv);
					if (lcol[1])
						glColor3ub(lcol[1]->r, lcol[1]->g, lcol[1]->b);
					else glColor3ub(0, 0, 0);
					glNormal3fv(ls[1]->v->no);
					glVertex3fv(ls[1]->v->co);

					if (luv[2])
						glTexCoord2fv(luv[2]->uv);
					if (lcol[2])
						glColor3ub(lcol[2]->r, lcol[2]->g, lcol[2]->b);
					else glColor3ub(0, 0, 0);
					glNormal3fv(ls[2]->v->no);
					glVertex3fv(ls[2]->v->co);
				}
				glEnd();
			}
		}
	}
}

static void bmDM_drawFacesTex(DerivedMesh *dm, int (*setDrawOptions)(MTFace *tface, int has_vcol, int matnr))
{
	bmDM_drawFacesTex_common(dm, setDrawOptions, NULL, NULL);
}

static void bmDM_drawMappedFacesTex(DerivedMesh *dm, int (*setDrawOptions)(void *userData, int index), void *userData)
{
	bmDM_drawFacesTex_common(dm, NULL, setDrawOptions, userData);
}

static void bmDM_drawMappedFacesGLSL(DerivedMesh *dm,
               int (*setMaterial)(int, void *attribs),
               int (*setDrawOptions)(void *userData, int index), void *userData) 
{
#if 0
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;
	BMesh *bm= bmdm->tc->bm;
	float (*vertexCos)[3]= bmdm->vertexCos;
	float (*vertexNos)[3]= bmdm->vertexNos;
	BMVert *eve;
	BMFace *efa;
	DMVertexAttribs attribs;
	GPUVertexAttribs gattribs;
	MTFace *tf;
	int transp, new_transp, orig_transp, tfoffset;
	int i, b, matnr, new_matnr, dodraw, layer;

	dodraw = 0;
	matnr = -1;

	transp = GPU_get_material_blend_mode();
	orig_transp = transp;
	layer = CustomData_get_layer_index(&bm->pdata, CD_MTFACE);
	tfoffset = (layer == -1)? -1: bm->pdata.layers[layer].offset;

	memset(&attribs, 0, sizeof(attribs));

	/* always use smooth shading even for flat faces, else vertex colors wont interpolate */
	glShadeModel(GL_SMOOTH);

	for (i=0,eve=bm->verts.first; eve; eve= eve->next)
		BMINDEX_SET(eve, i++);

#define PASSATTRIB(efa, eve, vert) {											\
	if(attribs.totorco) {														\
		float *orco = attribs.orco.array[BMINDEX_GET(eve)];							\
		glVertexAttrib3fvARB(attribs.orco.glIndex, orco);						\
	}																			\
	for(b = 0; b < attribs.tottface; b++) {										\
		MTFace *_tf = (MTFace*)((char*)efa->data + attribs.tface[b].bmOffset);	\
		glVertexAttrib2fvARB(attribs.tface[b].glIndex, _tf->uv[vert]);			\
	}																			\
	for(b = 0; b < attribs.totmcol; b++) {										\
		MCol *cp = (MCol*)((char*)efa->data + attribs.mcol[b].bmOffset);		\
		GLubyte col[4];															\
		col[0]= cp->b; col[1]= cp->g; col[2]= cp->r; col[3]= cp->a;				\
		glVertexAttrib4ubvARB(attribs.mcol[b].glIndex, col);					\
	}																			\
	if(attribs.tottang) {														\
		float *tang = attribs.tang.array[i*4 + vert];							\
		glVertexAttrib3fvARB(attribs.tang.glIndex, tang);						\
	}																			\
}

	for (i=0,efa= bm->faces.first; efa; i++,efa= efa->next) {
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
				if(vertexCos) glNormal3fv(bmdm->faceNos[i]);
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
#endif
}

static void bmDM_drawFacesGLSL(DerivedMesh *dm,
               int (*setMaterial)(int, void *attribs))
{
	dm->drawMappedFacesGLSL(dm, setMaterial, NULL, NULL);
}

static void bmDM_getMinMax(DerivedMesh *dm, float min_r[3], float max_r[3])
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;
	BMVert *eve;
	BMIter iter;
	int i;

	if (bmdm->tc->bm->verts.first) {
		eve = BMIter_New(&iter, bmdm->tc->bm, BM_VERTS_OF_MESH, NULL);
		for (i=0; eve; eve=BMIter_Step(&iter), i++) {
			if (bmdm->vertexCos) {
				DO_MINMAX(bmdm->vertexCos[i], min_r, max_r);
			} else {
				DO_MINMAX(eve->co, min_r, max_r);
			}
		}
	} else {
		min_r[0] = min_r[1] = min_r[2] = max_r[0] = max_r[1] = max_r[2] = 0.0;
	}
}
static int bmDM_getNumVerts(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;

	return bmdm->tc->bm->totvert;
}

static int bmDM_getNumEdges(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;

	return bmdm->tc->bm->totedge;
}

static int bmDM_getNumTessFaces(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;
	
	return bmdm->tc->tottri;
}

static int bmDM_getNumFaces(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;
	
	return bmdm->tc->bm->totface;
}

static int bmvert_to_mvert(BMVert *ev, MVert *vert_r)
{
	VECCOPY(vert_r->co, ev->co);

	vert_r->no[0] = (short)(ev->no[0] * 32767.0f);
	vert_r->no[1] = (short)(ev->no[1] * 32767.0f);
	vert_r->no[2] = (short)(ev->no[2] * 32767.0f);

	/* TODO what to do with vert_r->flag and vert_r->mat_nr? */
	vert_r->flag = BMFlags_To_MEFlags(ev);
	vert_r->mat_nr = 0;
	vert_r->bweight = (unsigned char) (ev->bweight*255.0f);
}

static void bmDM_getVert(DerivedMesh *dm, int index, MVert *vert_r)
{
	BMVert *ev;
	BMIter iter;
	int i;

	if (index < 0 || index >= ((EditDerivedBMesh *)dm)->tv) {
		printf("error in bmDM_getVert.\n");
		return;
	}

	ev = ((EditDerivedBMesh *)dm)->vtable[index];
	bmvert_to_mvert(ev, vert_r);
}

static void bmDM_getEdge(DerivedMesh *dm, int index, MEdge *edge_r)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = ((EditDerivedBMesh *)dm)->tc->bm;
	BMEdge *e;
	BMVert *ev, *v1, *v2;
	BMIter iter;
	int i;

	if (index < 0 || index >= ((EditDerivedBMesh *)dm)->te) {
		printf("error in bmDM_getEdge.\n");
		return;
	}

	e = bmdm->etable[index];

	edge_r->crease = (unsigned char) (e->crease*255.0f);
	edge_r->bweight = (unsigned char) (e->bweight*255.0f);
	/* TODO what to do with edge_r->flag? */
	edge_r->flag = ME_EDGEDRAW|ME_EDGERENDER;
	edge_r->flag |= BMFlags_To_MEFlags(e);
#if 0
	/* this needs setup of f2 field */
	if (!ee->f2) edge_r->flag |= ME_LOOSEEDGE;
#endif
	
	edge_r->v1 = GET_INT_FROM_POINTER(BLI_ghash_lookup(bmdm->vhash, e->v1));
	edge_r->v2 = GET_INT_FROM_POINTER(BLI_ghash_lookup(bmdm->vhash, e->v2));
}

static void bmDM_getTessFace(DerivedMesh *dm, int index, MFace *face_r)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = bmdm->tc->bm;
	BMFace *ef;
	BMIter iter;
	BMLoop **l;
	int i;
	
	if (index < 0 || index >= ((EditDerivedBMesh *)dm)->tf) {
		printf("error in bmDM_getTessFace.\n");
		return;
	}

	l = ((EditDerivedBMesh *)dm)->tc->looptris[index];

	ef = l[0]->f;

	face_r->mat_nr = (unsigned char) ef->mat_nr;
	face_r->flag = BMFlags_To_MEFlags(ef);

	face_r->v1 = GET_INT_FROM_POINTER(BLI_ghash_lookup(bmdm->vhash, l[0]->v));
	face_r->v2 = GET_INT_FROM_POINTER(BLI_ghash_lookup(bmdm->vhash, l[1]->v));
	face_r->v3 = GET_INT_FROM_POINTER(BLI_ghash_lookup(bmdm->vhash, l[2]->v));
	face_r->v4 = 0;

	test_index_face(face_r, NULL, 0, 3);
}

static void bmDM_copyVertArray(DerivedMesh *dm, MVert *vert_r)
{
	BMesh *bm = ((EditDerivedBMesh *)dm)->tc->bm;
	BMVert *ev;
	BMIter iter;

	ev = BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL);
	for( ; ev; ev = BMIter_Step(&iter), ++vert_r) {
		VECCOPY(vert_r->co, ev->co);

		vert_r->no[0] = (short) (ev->no[0] * 32767.0);
		vert_r->no[1] = (short) (ev->no[1] * 32767.0);
		vert_r->no[2] = (short) (ev->no[2] * 32767.0);

		/* TODO what to do with vert_r->flag and vert_r->mat_nr? */
		vert_r->mat_nr = 0;
		vert_r->flag = BMFlags_To_MEFlags(ev);
		vert_r->bweight = (unsigned char) (ev->bweight*255.0f);
	}
}

static void bmDM_copyEdgeArray(DerivedMesh *dm, MEdge *edge_r)
{
	BMesh *bm = ((EditDerivedBMesh *)dm)->tc->bm;
	BMEdge *ee;
	BMIter iter;
	BMVert *ev;
	int i;

	/* store vertex indices in tmp union */
	ev = BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL);
	for (i=0; ev; ev=BMIter_Step(&iter), i++)
		BMINDEX_SET(ev, i);

	ee = BMIter_New(&iter, bm, BM_EDGES_OF_MESH, NULL);
	for( ; ee; ee=BMIter_Step(&iter)) {
		edge_r->crease = (unsigned char) (ee->crease*255.0f);
		edge_r->bweight = (unsigned char) (ee->bweight*255.0f);
		/* TODO what to do with edge_r->flag? */
		edge_r->flag = ME_EDGEDRAW|ME_EDGERENDER;
		if (ee->head.flag & BM_SEAM) edge_r->flag |= ME_SEAM;
		if (ee->head.flag & BM_SHARP) edge_r->flag |= ME_SHARP;
#if 0
		/* this needs setup of f2 field */
		if (!ee->f2) edge_r->flag |= ME_LOOSEEDGE;
#endif

		edge_r->v1 = (int)BMINDEX_GET(ee->v1);
		edge_r->v2 = (int)BMINDEX_GET(ee->v2);
	}
}

static void bmDM_copyFaceArray(DerivedMesh *dm, MFace *face_r)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh *)dm;
	BMesh *bm = ((EditDerivedBMesh *)dm)->tc->bm;
	BMFace *ef;
	BMVert *ev;
	BMIter iter;
	BMLoop **l;
	int i;

	/* store vertexes indices in tmp union */
	ev = BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL);
	for (i=0; ev; ev=BMIter_Step(&iter), i++)
		BMINDEX_SET(ev, i);

	for (i=0; i<bmdm->tc->tottri; i++) {
		l = bmdm->tc->looptris[i];
		ef = l[0]->f;

		face_r->mat_nr = (unsigned char) ef->mat_nr;

		/*HACK/TODO: need to convert this*/
		face_r->flag = ef->head.flag;

		face_r->v1 = BMINDEX_GET(l[0]->v);
		face_r->v2 = BMINDEX_GET(l[1]->v);
		face_r->v3 = BMINDEX_GET(l[2]->v);
		face_r->v4 = 0;

		test_index_face(face_r, NULL, 0, 3);
	}
}

static void *bmDM_getFaceDataArray(DerivedMesh *dm, int type)
{
	EditDerivedBMesh *bmdm= (EditDerivedBMesh*) dm;
	BMesh *bm= bmdm->tc->bm;
	BMFace *efa;
	char *data, *bmdata;
	void *datalayer;
	int index, offset, size, i;

	datalayer = DM_get_tessface_data_layer(dm, type);
	if(datalayer)
		return datalayer;

	/* layers are store per face for editmesh, we convert to a tbmporary
	 * data layer array in the derivedmesh when these are requested */
	if(type == CD_MTFACE || type == CD_MCOL) {
		index = CustomData_get_layer_index(&bm->pdata, type);

		if(index != -1) {
			offset = bm->pdata.layers[index].offset;
			size = CustomData_sizeof(type);

			DM_add_tessface_layer(dm, type, CD_CALLOC, NULL);
			index = CustomData_get_layer_index(&dm->faceData, type);
			dm->faceData.layers[index].flag |= CD_FLAG_TEMPORARY;

			data = datalayer = DM_get_tessface_data_layer(dm, type);
			for (i=0; i<bmdm->tc->tottri; i++, data+=size) {
				efa = bmdm->tc->looptris[i][0]->f;
				/*BMESH_TODO: need to still add tface data,
				  derived from the loops.*/
				bmdata = CustomData_bmesh_get(&bm->pdata, efa->head.data, type);
				memcpy(data, bmdata, size);
			}
		}
	}

	return datalayer;
}

typedef struct bmDM_loopIter {
	DMLoopIter head;

	BMFace *f;
	BMLoop *l, *nextl;
	BMIter iter;
	BMesh *bm;
} bmDM_loopIter;

typedef struct bmDM_faceIter {
	DMFaceIter head;

	BMFace *f, *nextf;
	BMIter iter;
	BMesh *bm;

	bmDM_loopIter loopiter;
} bmDM_faceIter;

void bmDM_faceIterStep(void *self)
{
	bmDM_faceIter *iter = self;
	
	iter->f = iter->nextf;

	iter->head.mat_nr = iter->f->mat_nr;
	iter->head.flags = BMFlags_To_MEFlags(iter->f);
	iter->head.index++;

	iter->head.len = iter->f->len;

	iter->nextf = BMIter_Step(&iter->iter);

	if (!iter->nextf) iter->head.done = 1;
}

void *bmDM_getFaceCDData(void *self, int type, int layer)
{
	bmDM_faceIter *iter = self;

	if (layer == -1) 
		return CustomData_bmesh_get(&iter->bm->pdata, iter->f->head.data, type);
	else return CustomData_bmesh_get_n(&iter->bm->pdata, iter->f->head.data, type, layer);
}

void bmDM_loopIterStep(void *self)
{
	bmDM_loopIter *iter = self;

	iter->l = BMIter_Step(&iter->iter);
	if (!iter->l) {
		iter->head.done = 1;
		return;
	}

	bmvert_to_mvert(iter->l->v, &iter->head.v);
	iter->head.index++;
	iter->head.vindex = BMINDEX_GET(iter->l->v);
	iter->head.eindex = BMINDEX_GET(iter->l->e);
}

void *bmDM_getLoopCDData(void *self, int type, int layer)
{
	bmDM_loopIter *iter = self;

	if (layer == -1) 
		return CustomData_bmesh_get(&iter->bm->ldata, iter->l->head.data, type);
	else return CustomData_bmesh_get_n(&iter->bm->ldata, iter->l->head.data, type, layer);
}

void *bmDM_getVertCDData(void *self, int type, int layer)
{
	bmDM_loopIter *iter = self;

	if (layer == -1) 
		return CustomData_bmesh_get(&iter->bm->vdata, iter->l->v->head.data, type);
	else return CustomData_bmesh_get_n(&iter->bm->vdata, iter->l->v->head.data, type, layer);
}

void bmDM_iterFree(void *self)
{
	MEM_freeN(self);
}

void bmDM_nulliterFree(void *self)
{
}

DMLoopIter *bmDM_newLoopsIter(void *faceiter)
{
	bmDM_faceIter *fiter = faceiter;
	bmDM_loopIter *iter = &fiter->loopiter;

	memset(&fiter->loopiter, 0, sizeof(bmDM_loopIter));

	iter->bm = fiter->bm;
	iter->f = fiter->f;
	iter->l = BMIter_New(&iter->iter, iter->bm, BM_LOOPS_OF_FACE, iter->f);

	iter->head.step = bmDM_loopIterStep;
	iter->head.getLoopCDData = bmDM_getLoopCDData;
	iter->head.getVertCDData = bmDM_getVertCDData;

	bmvert_to_mvert(iter->l->v, &iter->head.v);
	iter->head.vindex = BMINDEX_GET(iter->l->v);
	iter->head.eindex = BMINDEX_GET(iter->l->e);

	return (DMLoopIter*) iter;
}

static DMFaceIter *bmDM_getFaceIter(void *dm)
{
	EditDerivedBMesh *bmdm= dm;
	bmDM_faceIter *iter = MEM_callocN(sizeof(bmDM_faceIter), "bmDM_faceIter");
	BMIter biter;
	BMVert *v;
	BMEdge *e;
	int i;

	iter->bm = bmdm->tc->bm;
	iter->f = iter->nextf = BMIter_New(&iter->iter, iter->bm, BM_FACES_OF_MESH, NULL);
	
	iter->head.step = bmDM_faceIterStep;
	iter->head.free = bmDM_iterFree;
	iter->head.getCDData = bmDM_getFaceCDData;
	iter->head.getLoopsIter = bmDM_newLoopsIter;
	
	iter->head.mat_nr = iter->f->mat_nr;
	iter->head.flags = BMFlags_To_MEFlags(iter->f);

	/*set up vert/edge indices*/
	i = 0;
	BM_ITER(v, &biter, iter->bm, BM_VERTS_OF_MESH, NULL) {
		BMINDEX_SET(v, i);
		i++;
	}

	i = 0;
	BM_ITER(e, &biter, iter->bm, BM_EDGES_OF_MESH, NULL) {
		BMINDEX_SET(e, i);
		i++;
	}

	return (DMFaceIter*) iter;
}

static void bmDM_release(void *dm)
{
	EditDerivedBMesh *bmdm= dm;

	if (DM_release(dm)) {
		if (bmdm->vertexCos) {
			MEM_freeN(bmdm->vertexCos);
			MEM_freeN(bmdm->vertexNos);
			MEM_freeN(bmdm->faceNos);
		}
		
		if (bmdm->fhash) BLI_ghash_free(bmdm->fhash, NULL, NULL);
		if (bmdm->ehash) BLI_ghash_free(bmdm->ehash, NULL, NULL);
		if (bmdm->vhash) BLI_ghash_free(bmdm->vhash, NULL, NULL);

		if (bmdm->vtable) MEM_freeN(bmdm->vtable);
		if (bmdm->etable) MEM_freeN(bmdm->etable);
		if (bmdm->ftable) MEM_freeN(bmdm->ftable);
		
		MEM_freeN(bmdm);
	}
}

CustomData *bmDm_getVertDataLayout(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh*)dm;

	return &bmdm->tc->bm->vdata;
}

CustomData *bmDm_getEdgeDataLayout(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh*)dm;

	return &bmdm->tc->bm->edata;
}

CustomData *bmDm_getTessFaceDataLayout(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh*)dm;

	return &bmdm->tessface_layout;
}

CustomData *bmDm_getLoopDataLayout(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh*)dm;

	return &bmdm->tc->bm->ldata;
}

CustomData *bmDm_getFaceDataLayout(DerivedMesh *dm)
{
	EditDerivedBMesh *bmdm = (EditDerivedBMesh*)dm;

	return &bmdm->tc->bm->pdata;
}


DerivedMesh *getEditDerivedBMesh(BMEditMesh *em, Object *ob,
                                           float (*vertexCos)[3])
{
	EditDerivedBMesh *bmdm = MEM_callocN(sizeof(*bmdm), "bmdm");
	BMesh *bm = em->bm;
	int i;
	
	bmdm->tc = em;

	DM_init((DerivedMesh*)bmdm, em->bm->totvert, em->bm->totedge, em->tottri,
		 em->bm->totloop, em->bm->totface);
	
	for (i=0; i<bm->ldata.totlayer; i++) {
		if (bm->ldata.layers[i].type == CD_MLOOPCOL) {
			CustomData_add_layer(&bmdm->tessface_layout, CD_MCOL, CD_ASSIGN, NULL, 0);
		} else if (bm->ldata.layers[i].type == CD_MLOOPUV) {
			CustomData_add_layer(&bmdm->tessface_layout, CD_MTFACE, CD_ASSIGN, NULL, 0);
		}
	}

	bmdm->dm.numVertData = bm->totvert;
	bmdm->dm.numEdgeData = bm->totedge;
	bmdm->dm.numFaceData = em->tottri;
	bmdm->dm.numLoopData = bm->totloop;
	bmdm->dm.numPolyData = bm->totface;

	bmdm->dm.getMinMax = bmDM_getMinMax;

	bmdm->dm.getVertDataLayout = bmDm_getVertDataLayout;
	bmdm->dm.getEdgeDataLayout = bmDm_getEdgeDataLayout;
	bmdm->dm.getTessFaceDataLayout = bmDm_getTessFaceDataLayout;
	bmdm->dm.getLoopDataLayout = bmDm_getLoopDataLayout;
	bmdm->dm.getFaceDataLayout = bmDm_getFaceDataLayout;

	bmdm->dm.getNumVerts = bmDM_getNumVerts;
	bmdm->dm.getNumEdges = bmDM_getNumEdges;
	bmdm->dm.getNumTessFaces = bmDM_getNumTessFaces;
	bmdm->dm.getNumFaces = bmDM_getNumFaces;

	bmdm->dm.getVert = bmDM_getVert;
	bmdm->dm.getEdge = bmDM_getEdge;
	bmdm->dm.getTessFace = bmDM_getTessFace;
	bmdm->dm.copyVertArray = bmDM_copyVertArray;
	bmdm->dm.copyEdgeArray = bmDM_copyEdgeArray;
	bmdm->dm.copyTessFaceArray = bmDM_copyFaceArray;
	bmdm->dm.getTessFaceDataArray = bmDM_getFaceDataArray;

	bmdm->dm.newFaceIter = bmDM_getFaceIter;
	bmdm->dm.recalcTesselation = bmDM_recalcTesselation;

	bmdm->dm.foreachMappedVert = bmDM_foreachMappedVert;
	bmdm->dm.foreachMappedEdge = bmDM_foreachMappedEdge;
	bmdm->dm.foreachMappedFaceCenter = bmDM_foreachMappedFaceCenter;

	bmdm->dm.drawEdges = bmDM_drawEdges;
	bmdm->dm.drawMappedEdges = bmDM_drawMappedEdges;
	bmdm->dm.drawMappedEdgesInterp = bmDM_drawMappedEdgesInterp;
	bmdm->dm.drawMappedFaces = bmDM_drawMappedFaces;
	bmdm->dm.drawMappedFacesTex = bmDM_drawMappedFacesTex;
	bmdm->dm.drawMappedFacesGLSL = bmDM_drawMappedFacesGLSL;
	bmdm->dm.drawFacesTex = bmDM_drawFacesTex;
	bmdm->dm.drawFacesGLSL = bmDM_drawFacesGLSL;
	bmdm->dm.drawUVEdges = bmDM_drawUVEdges;

	bmdm->dm.release = bmDM_release;
	
	bmdm->vertexCos = vertexCos;

	if(CustomData_has_layer(&bm->vdata, CD_MDEFORMVERT)) {
		BMIter iter;
		BMVert *eve;
		int i;

		DM_add_vert_layer(&bmdm->dm, CD_MDEFORMVERT, CD_CALLOC, NULL);
		
		eve = BMIter_New(&iter, bmdm->tc->bm, BM_VERTS_OF_MESH, NULL);
		for (i=0; eve; eve=BMIter_Step(&iter), i++)
			DM_set_vert_data(&bmdm->dm, i, CD_MDEFORMVERT,
			                 CustomData_bmesh_get(&bm->vdata, eve->head.data, CD_MDEFORMVERT));
	}

	if(vertexCos) {
		BMVert *eve;
		BMIter iter;
		int totface = bmdm->tc->tottri;
		int i;
		
		eve=BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL);
		for (i=0; eve; eve=BMIter_Step(&iter), i++)
			BMINDEX_SET(eve, i);

		bmdm->vertexNos = MEM_callocN(sizeof(*bmdm->vertexNos)*i, "bmdm_vno");
		bmdm->faceNos = MEM_mallocN(sizeof(*bmdm->faceNos)*totface, "bmdm_vno");

		for (i=0; i<bmdm->tc->tottri; i++) {
			BMLoop **l = bmdm->tc->looptris[i];
			float *v1 = vertexCos[(int) BMINDEX_GET(l[0]->v)];
			float *v2 = vertexCos[(int) BMINDEX_GET(l[1]->v)];
			float *v3 = vertexCos[(int) BMINDEX_GET(l[2]->v)];
			float *no = bmdm->faceNos[i];
			
			CalcNormFloat(v1, v2, v3, no);
			VecAddf(bmdm->vertexNos[BMINDEX_GET(l[0]->v)], bmdm->vertexNos[BMINDEX_GET(l[0]->v)], no);
			VecAddf(bmdm->vertexNos[BMINDEX_GET(l[1]->v)], bmdm->vertexNos[BMINDEX_GET(l[1]->v)], no);
			VecAddf(bmdm->vertexNos[BMINDEX_GET(l[2]->v)], bmdm->vertexNos[BMINDEX_GET(l[2]->v)], no);
		}

		eve=BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL);
		for (i=0; eve; eve=BMIter_Step(&iter), i++) {
			float *no = bmdm->vertexNos[i];
			/* following Mesh convention; we use vertex coordinate itself
			 * for normal in this case */
			if (Normalize(no)==0.0) {
				VECCOPY(no, vertexCos[i]);
				Normalize(no);
			}
		}
	}

	//bmdm_recalc_lookups(bmdm);

	return (DerivedMesh*) bmdm;
}
