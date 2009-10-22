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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_image_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_array.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"
#include "BKE_tessmesh.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_view2d.h"

#include "uvedit_intern.h"

#define EFA_F1_FLAG	2

/************************* state testing ************************/

int ED_uvedit_test(Object *obedit)
{
	BMEditMesh *em;
	int ret;

	if(obedit->type != OB_MESH)
		return 0;

	em = ((Mesh*)obedit->data)->edit_btmesh;
	ret = EDBM_texFaceCheck(em);
	
	return ret;
}

/************************* assign image ************************/

void ED_uvedit_assign_image(Scene *scene, Object *obedit, Image *ima, Image *previma)
{
	BMEditMesh *em;
	BMFace *efa;
	BMIter iter;
	MTexPoly *tf;
	int update= 0;
	
	/* skip assigning these procedural images... */
	if(ima && (ima->type==IMA_TYPE_R_RESULT || ima->type==IMA_TYPE_COMPOSITE))
		return;

	/* verify we have a mesh we can work with */
	if(!obedit || (obedit->type != OB_MESH))
		return;

	em= ((Mesh*)obedit->data)->edit_btmesh;
	if(!em || !em->bm->totface) {
		return;
	}
	
	/* ensure we have a uv layer */
	if(!CustomData_has_layer(&em->bm->pdata, CD_MTEXPOLY)) {
		BM_add_data_layer(em->bm, &em->bm->pdata, CD_MTEXPOLY);
		BM_add_data_layer(em->bm, &em->bm->ldata, CD_MLOOPUV);
		update= 1;
	}

	/* now assign to all visible faces */
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

		if(uvedit_face_visible(scene, previma, efa, tf)) {
			if(ima) {
				tf->tpage= ima;
				tf->mode |= TF_TEX;
				
				if(ima->id.us==0) id_us_plus(&ima->id);
				else id_lib_extern(&ima->id);
			}
			else {
				tf->tpage= NULL;
				tf->mode &= ~TF_TEX;
			}

			update = 1;
		}
	}

	/* and update depdency graph */
	if(update)
		DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
}

/* dotile -	1, set the tile flag (from the space image)
 * 			2, set the tile index for the faces. */
void ED_uvedit_set_tile(bContext *C, Scene *scene, Object *obedit, Image *ima, int curtile)
{
	BMEditMesh *em;
	BMFace *efa;
	BMIter iter;
	MTexPoly *tf;
	
	/* verify if we have something to do */
	if(!ima || !ED_uvedit_test(obedit))
		return;
	
	/* skip assigning these procedural images... */
	if(ima->type==IMA_TYPE_R_RESULT || ima->type==IMA_TYPE_COMPOSITE)
		return;
	
	em= ((Mesh*)obedit->data)->edit_btmesh;

	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

		if(!BM_TestHFlag(efa, BM_HIDDEN) && BM_TestHFlag(efa, BM_SELECT))
			tf->tile= curtile; /* set tile index */
	}

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);
}

/*********************** space conversion *********************/

static void uvedit_pixel_to_float(SpaceImage *sima, float *dist, float pixeldist)
{
	int width, height;

	ED_space_image_size(sima, &width, &height);

	dist[0]= pixeldist/width;
	dist[1]= pixeldist/height;
}

/*************** visibility and selection utilities **************/

int uvedit_face_visible_nolocal(Scene *scene, BMFace *efa)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION)
		return (BM_TestHFlag(efa, BM_HIDDEN)==0);
	else
		return (BM_TestHFlag(efa, BM_HIDDEN)==0 && BM_TestHFlag(efa, BM_SELECT));
}

int uvedit_face_visible(Scene *scene, Image *ima, BMFace *efa, MTexPoly *tf) {
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SHOW_SAME_IMAGE)
		return (tf->tpage==ima)? uvedit_face_visible_nolocal(scene, efa): 0;
	else
		return uvedit_face_visible_nolocal(scene, efa);
}

int uvedit_face_selected(Scene *scene, BMEditMesh *em, BMFace *efa)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION)
		return (BM_TestHFlag(efa, BM_SELECT));
	else {
		BMLoop *l;
		MLoopUV *luv;
		BMIter liter;

		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			if (!(luv->flag & MLOOPUV_VERTSEL))
				return 0;
		}

		return 1;
	}
}

int uvedit_face_select(Scene *scene, BMEditMesh *em, BMFace *efa)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION)
		BM_Select(em->bm, efa, 1);
	else {
		BMLoop *l;
		MLoopUV *luv;
		BMIter liter;

		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			luv->flag |= MLOOPUV_VERTSEL;
		}

		return 1;
	}

	return 0;
}

int uvedit_face_deselect(Scene *scene, BMEditMesh *em, BMFace *efa)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION)
		BM_Select(em->bm, efa, 0);
	else {
		BMLoop *l;
		MLoopUV *luv;
		BMIter liter;

		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			luv->flag &= ~MLOOPUV_VERTSEL;
		}

		return 1;
	}

	return 0;
}

int uvedit_edge_selected(BMEditMesh *em, Scene *scene, BMLoop *l)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		if(ts->selectmode == SCE_SELECT_FACE)
			return BM_TestHFlag(l->f, BM_SELECT);
		else if(ts->selectmode == SCE_SELECT_EDGE) {
			return BM_TestHFlag(l->e, BM_SELECT);
		} else
			return BM_TestHFlag(l->v, BM_SELECT) && 
			       BM_TestHFlag(((BMLoop*)l->head.next)->v, BM_SELECT);
	}
	else {
		MLoopUV *luv1, *luv2;

		luv1 = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		luv2 = CustomData_bmesh_get(&em->bm->ldata, l->head.next->data, CD_MLOOPUV);

		return (luv1->flag & MLOOPUV_VERTSEL) && (luv2->flag & MLOOPUV_VERTSEL);
	}
}

void uvedit_edge_select(BMEditMesh *em, Scene *scene, BMLoop *l)

{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		if(ts->selectmode == SCE_SELECT_FACE)
			BM_Select(em->bm, l->f, 1);
		else if(ts->selectmode == SCE_SELECT_EDGE)
			BM_Select(em->bm, l->e, 1);
		else {
			BM_Select(em->bm, l->e->v1, 1);
			BM_Select(em->bm, l->e->v2, 1);
		}
	}
	else {
		MLoopUV *luv1, *luv2;

		luv1 = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		luv2 = CustomData_bmesh_get(&em->bm->ldata, l->head.next->data, CD_MLOOPUV);
		
		luv1->flag |= MLOOPUV_VERTSEL;
		luv2->flag |= MLOOPUV_VERTSEL;
	}
}

void uvedit_edge_deselect(BMEditMesh *em, Scene *scene, BMLoop *l)

{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		if(ts->selectmode == SCE_SELECT_FACE)
			BM_Select(em->bm, l->f, 0);
		else if(ts->selectmode == SCE_SELECT_EDGE)
			BM_Select(em->bm, l->e, 0);
		else {
			BM_Select(em->bm, l->e->v1, 0);
			BM_Select(em->bm, l->e->v2, 0);
		}
	}
	else {
		MLoopUV *luv1, *luv2;

		luv1 = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		luv2 = CustomData_bmesh_get(&em->bm->ldata, l->head.next->data, CD_MLOOPUV);
		
		luv1->flag &= ~MLOOPUV_VERTSEL;
		luv2->flag &= ~MLOOPUV_VERTSEL;
	}
}

int uvedit_uv_selected(BMEditMesh *em, Scene *scene, BMLoop *l)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		if(ts->selectmode == SCE_SELECT_FACE)
			return BM_TestHFlag(l->f, BM_SELECT);
		else
			return BM_TestHFlag(l, BM_SELECT);
	}
	else {
		MLoopUV *luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

		return luv->flag & MLOOPUV_VERTSEL;
	}
}

void uvedit_uv_select(BMEditMesh *em, Scene *scene, BMLoop *l)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		if(ts->selectmode == SCE_SELECT_FACE)
			BM_Select(em->bm, l->f, 1);
		else
			BM_Select(em->bm, l->v, 1);
	}
	else {
		MLoopUV *luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		
		luv->flag |= MLOOPUV_VERTSEL;
	}
}

void uvedit_uv_deselect(BMEditMesh *em, Scene *scene, BMLoop *l)
{
	ToolSettings *ts= scene->toolsettings;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		if(ts->selectmode == SCE_SELECT_FACE)
			BM_Select(em->bm, l->f, 0);
		else
			BM_Select(em->bm, l->v, 0);
	}
	else {
		MLoopUV *luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		
		luv->flag &= ~MLOOPUV_VERTSEL;
	}
}

/*********************** geometric utilities ***********************/
void poly_uv_center(BMEditMesh *em, BMFace *f, float cent[2])
{
	BMLoop *l;
	MLoopUV *luv;
	BMIter liter;

	cent[0] = cent[1] = 0.0f;

	BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, f) {
		luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		cent[0] += luv->uv[0];
		cent[1] += luv->uv[1];
	}

	cent[0] /= (float) f->len;
	cent[1] /= (float) f->len;
}


void uv_center(float uv[][2], float cent[2], int quad)
{
	if(quad) {
		cent[0] = (uv[0][0] + uv[1][0] + uv[2][0] + uv[3][0]) / 4.0;
		cent[1] = (uv[0][1] + uv[1][1] + uv[2][1] + uv[3][1]) / 4.0;		
	}
	else {
		cent[0] = (uv[0][0] + uv[1][0] + uv[2][0]) / 3.0;
		cent[1] = (uv[0][1] + uv[1][1] + uv[2][1]) / 3.0;		
	}
}

float uv_area(float uv[][2], int quad)
{
	if(quad)
		return AreaF2Dfl(uv[0], uv[1], uv[2]) + AreaF2Dfl(uv[0], uv[2], uv[3]); 
	else
		return AreaF2Dfl(uv[0], uv[1], uv[2]); 
}

float poly_uv_area(float uv[][2], int len)
{
	//BMESH_TODO: make this not suck
	//maybe use scanfill? I dunno.

	if(len >= 4)
		return AreaF2Dfl(uv[0], uv[1], uv[2]) + AreaF2Dfl(uv[0], uv[2], uv[3]); 
	else
		return AreaF2Dfl(uv[0], uv[1], uv[2]); 

	return 1.0;
}

void poly_copy_aspect(float uv_orig[][2], float uv[][2], float aspx, float aspy, int len)
{
	int i;
	for (i=0; i<len; i++) {
		uv[i][0] = uv_orig[i][0]*aspx;
		uv[i][1] = uv_orig[i][1]*aspy;
	}
}

void uv_copy_aspect(float uv_orig[][2], float uv[][2], float aspx, float aspy)
{
	uv[0][0] = uv_orig[0][0]*aspx;
	uv[0][1] = uv_orig[0][1]*aspy;
	
	uv[1][0] = uv_orig[1][0]*aspx;
	uv[1][1] = uv_orig[1][1]*aspy;
	
	uv[2][0] = uv_orig[2][0]*aspx;
	uv[2][1] = uv_orig[2][1]*aspy;
	
	uv[3][0] = uv_orig[3][0]*aspx;
	uv[3][1] = uv_orig[3][1]*aspy;
}

int ED_uvedit_minmax(Scene *scene, Image *ima, Object *obedit, float *min, float *max)
{
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	int sel;

	INIT_MINMAX2(min, max);

	sel= 0;
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if(!uvedit_face_visible(scene, ima, efa, tf))
			continue;
		
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			if (uvedit_uv_selected(em, scene, l)) 
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				DO_MINMAX2(luv->uv, min, max); 
				sel = 1;
			}
	}

	return sel;
}

int uvedit_center(Scene *scene, Image *ima, Object *obedit, 
		  float *cent, int mode)
{
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	float min[2], max[2];
	int change= 0;
	
	if(mode==0) {
		if(ED_uvedit_minmax(scene, ima, obedit, min, max))
			change = 1;
	}
	else if(mode==1) {
		INIT_MINMAX2(min, max);
		
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if(!uvedit_face_visible(scene, ima, efa, tf))
				continue;
			
			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				if (uvedit_uv_selected(em, scene, l)) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
	
					DO_MINMAX2(luv->uv, min, max); 
					change= 1;
				}
			}
		}
	}
	
	if(change) {
		cent[0]= (min[0]+max[0])/2.0;
		cent[1]= (min[1]+max[1])/2.0;
		
		return 1;
	}

	return 0;
}

/************************** find nearest ****************************/

typedef struct NearestHit {
	BMFace *efa;
	MTexPoly *tf;
	BMLoop *l, *nextl;
	MLoopUV *luv, *nextluv;
	int lindex; //index of loop within face
	int vert1, vert2; //index in mesh of edge vertices
} NearestHit;

static void find_nearest_uv_edge(Scene *scene, Image *ima, BMEditMesh *em, float co[2], NearestHit *hit)
{
	MTexPoly *tf;
	BMFace *efa;
	BMLoop *l;
	BMVert *eve;
	BMIter iter, liter;
	MLoopUV *luv, *nextluv;
	float mindist, dist;
	int i;

	mindist= 1e10f;
	memset(hit, 0, sizeof(*hit));

	eve = BMIter_New(&iter, em->bm, BM_VERTS_OF_MESH, NULL);
	for (i=0; eve; eve=BMIter_Step(&iter), i++) {
		BMINDEX_SET(eve, i);
	}
	
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tf= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if(!uvedit_face_visible(scene, ima, efa, tf))
			continue;
		
		i = 0;
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			nextluv = CustomData_bmesh_get(&em->bm->ldata, l->head.next->data, CD_MLOOPUV);

			dist= PdistVL2Dfl(co, luv->uv, nextluv->uv);

			if(dist < mindist) {
				hit->tf= tf;
				hit->efa= efa;
				
				hit->l = l;
				hit->nextl = (BMLoop*)l->head.next;
				hit->luv = luv;
				hit->nextluv = nextluv;
				hit->lindex = i;
				hit->vert1 = BMINDEX_GET(hit->l->v);
				hit->vert2 = BMINDEX_GET(((BMLoop*)hit->l->head.next)->v);

				mindist = dist;
			}

			i++;
		}
	}
}

static void find_nearest_uv_face(Scene *scene, Image *ima, BMEditMesh *em, float co[2], NearestHit *hit)
{
	MTexPoly *tf;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	float mindist, dist, cent[2];

	mindist= 1e10f;
	memset(hit, 0, sizeof(*hit));

	/*this will fill in hit.vert1 and hit.vert2*/
	find_nearest_uv_edge(scene, ima, em, co, hit);
	hit->l = hit->nextl = NULL;
	hit->luv = hit->nextluv = NULL;

	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tf= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if(!uvedit_face_visible(scene, ima, efa, tf))
			continue;
		
		cent[0]= cent[1]= 0.0f;
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

			cent[0] += luv->uv[0];
			cent[1] += luv->uv[1];
		}

		cent[0] /= efa->len;
		cent[1] /= efa->len;
		dist= fabs(co[0]- cent[0]) + fabs(co[1]- cent[1]);

		if(dist < mindist) {
			hit->tf= tf;
			hit->efa= efa;
			mindist= dist;
		}
	}
}

static int nearest_uv_between(BMEditMesh *em, BMFace *efa, int nverts, int id, 
			      float co[2], float uv[2])
{
	BMLoop *l;
	MLoopUV *luv;
	BMIter iter;
	float m[3], v1[3], v2[3], c1, c2, *uv1, *uv2, *uv3;
	int id1, id2, i;

	id1= (id+efa->len-1)%efa->len;
	id2= (id+efa->len+1)%efa->len;

	m[0]= co[0]-uv[0];
	m[1]= co[1]-uv[1];

	i = 0;
	BM_ITER(l, &iter, em->bm, BM_LOOPS_OF_FACE, efa) {
		luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
		
		if (i == id1)
			uv1 = luv->uv;
		else if (i == id)
			uv2 = luv->uv;
		else if (i == id2)
			uv3 = luv->uv;

		i++;
	}

	VecSubf(v1, uv1, uv);
	VecSubf(v2, uv3, uv);

	/* m and v2 on same side of v-v1? */
	c1= v1[0]*m[1] - v1[1]*m[0];
	c2= v1[0]*v2[1] - v1[1]*v2[0];

	if(c1*c2 < 0.0f)
		return 0;

	/* m and v1 on same side of v-v2? */
	c1= v2[0]*m[1] - v2[1]*m[0];
	c2= v2[0]*v1[1] - v2[1]*v1[0];

	return (c1*c2 >= 0.0f);
}

static void find_nearest_uv_vert(Scene *scene, Image *ima, BMEditMesh *em, 
				 float co[2], float penalty[2], NearestHit *hit)
{
	BMFace *efa;
	BMVert *eve;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	float mindist, dist;
	int i, nverts;

	/*this will fill in hit.vert1 and hit.vert2*/
	find_nearest_uv_edge(scene, ima, em, co, hit);
	hit->l = hit->nextl = NULL;
	hit->luv = hit->nextluv = NULL;

	mindist= 1e10f;
	memset(hit, 0, sizeof(*hit));
	
	eve = BMIter_New(&iter, em->bm, BM_VERTS_OF_MESH, NULL);
	for (i=0; eve; eve=BMIter_Step(&iter), i++) {
		BMINDEX_SET(eve, i);
	}

	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tf= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if(!uvedit_face_visible(scene, ima, efa, tf))
			continue;
		
		i = 0;
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

			if(penalty && uvedit_uv_selected(em, scene, l))
				dist= fabs(co[0]-luv->uv[0])+penalty[0] + fabs(co[1]-luv->uv[1])+penalty[1];
			else
				dist= fabs(co[0]-luv->uv[0]) + fabs(co[1]-luv->uv[1]);

			if(dist<=mindist) {
				if(dist==mindist)
					if(!nearest_uv_between(em, efa, efa->len, i, co, luv->uv)) {
						i++;
						continue;
					}

				mindist= dist;

				hit->l = l;
				hit->nextl = (BMLoop*)l->head.next;
				hit->luv = luv;
				hit->nextluv = CustomData_bmesh_get(&em->bm->ldata, l->head.next->data, CD_MLOOPUV);
				hit->tf= tf;
				hit->efa= efa;
				hit->lindex = i;
				hit->vert1 = BMINDEX_GET(hit->l->v);
			}

			i++;
		}
	}
}

int ED_uvedit_nearest_uv(Scene *scene, Object *obedit, Image *ima, float co[2], float uv[2])
{
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	float mindist, dist;
	int found= 0;

	mindist= 1e10f;
	uv[0]= co[0];
	uv[1]= co[1];
	
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tf= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if(!uvedit_face_visible(scene, ima, efa, tf))
			continue;
		
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			dist= fabs(co[0]-luv->uv[0]) + fabs(co[1]-luv->uv[1]);

			if(dist<=mindist) {
				mindist= dist;

				uv[0]= luv->uv[0];
				uv[1]= luv->uv[1];
				found= 1;
			}
		}
	}

	return found;
}

/*********************** loop select ***********************/

static void uv_vertex_loop_flag(UvMapVert *first)
{
	UvMapVert *iterv;
	int count= 0;

	for(iterv=first; iterv; iterv=iterv->next) {
		if(iterv->separate && iterv!=first)
			break;

		count++;
	}
	
	if(count < 5)
		first->flag= 1;
}

static UvMapVert *uv_vertex_map_get(UvVertMap *vmap, BMFace *efa, int a)
{
	UvMapVert *iterv, *first;
	BMLoop *l;

	l = BMIter_AtIndex(NULL, BM_LOOPS_OF_FACE, efa, a);
	first= EDBM_get_uv_map_vert(vmap,  BMINDEX_GET(l->v));

	for(iterv=first; iterv; iterv=iterv->next) {
		if(iterv->separate)
			first= iterv;
		if(iterv->f == BMINDEX_GET(efa))
			return first;
	}
	
	return NULL;
}

static int uv_edge_tag_faces(BMEditMesh *em, UvMapVert *first1, UvMapVert *first2, int *totface)
{
	UvMapVert *iterv1, *iterv2;
	BMFace *efa;
	int tot = 0;

	/* count number of faces this edge has */
	for(iterv1=first1; iterv1; iterv1=iterv1->next) {
		if(iterv1->separate && iterv1 != first1)
			break;

		for(iterv2=first2; iterv2; iterv2=iterv2->next) {
			if(iterv2->separate && iterv2 != first2)
				break;

			if(iterv1->f == iterv2->f) {
				/* if face already tagged, don't do this edge */
				efa= EDBM_get_face_for_index(em, iterv1->f);
				if(BMO_TestFlag(em->bm, efa, EFA_F1_FLAG))
					return 0;

				tot++;
				break;
			}
		}
	}

	if(*totface == 0) /* start edge */
		*totface= tot;
	else if(tot != *totface) /* check for same number of faces as start edge */
		return 0;

	/* tag the faces */
	for(iterv1=first1; iterv1; iterv1=iterv1->next) {
		if(iterv1->separate && iterv1 != first1)
			break;

		for(iterv2=first2; iterv2; iterv2=iterv2->next) {
			if(iterv2->separate && iterv2 != first2)
				break;

			if(iterv1->f == iterv2->f) {
				efa= EDBM_get_face_for_index(em, iterv1->f);
				BMO_SetFlag(em->bm, efa, EFA_F1_FLAG);
				break;
			}
		}
	}

	return 1;
}

static int select_edgeloop(Scene *scene, Image *ima, BMEditMesh *em, NearestHit *hit, float limit[2], int extend)
{
	BMVert *eve;
	BMFace *efa;
	BMIter iter, liter;
	BMLoop *l;
	MTexPoly *tf;
	UvVertMap *vmap;
	UvMapVert *iterv1, *iterv2;
	int a, count, looking, nverts, starttotf, select;

	/* setup */
	EDBM_init_index_arrays(em, 0, 0, 1);
	vmap= EDBM_make_uv_vert_map(em, 0, 0, limit);

	count = 0;
	BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		BMINDEX_SET(eve, count);
		count++;
	}

	count = 0;
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if(!extend) {
			uvedit_face_deselect(scene, em, efa);
		}
		
		BMO_ClearFlag(em->bm, efa, EFA_F1_FLAG);
		BMINDEX_SET(efa, count);
		count++;
	}

	/* set flags for first face and verts */
	nverts= hit->efa->len;
	iterv1= uv_vertex_map_get(vmap, hit->efa, hit->lindex);
	iterv2= uv_vertex_map_get(vmap, hit->efa, (hit->lindex+1)%nverts);
	uv_vertex_loop_flag(iterv1);
	uv_vertex_loop_flag(iterv2);

	starttotf= 0;
	uv_edge_tag_faces(em, iterv1, iterv2, &starttotf);

	/* sorry, first edge isnt even ok */
	if(iterv1->flag==0 && iterv2->flag==0) looking= 0;
	else looking= 1;

	/* iterate */
	while(looking) {
		looking= 0;

		/* find correct valence edges which are not tagged yet, but connect to tagged one */

		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			if(!BMO_TestFlag(em->bm, efa, EFA_F1_FLAG) && uvedit_face_visible(scene, ima, efa, tf)) {
				nverts= efa->len;
				for(a=0; a<nverts; a++) {
					/* check face not hidden and not tagged */
					iterv1= uv_vertex_map_get(vmap, efa, a);
					iterv2= uv_vertex_map_get(vmap, efa, (a+1)%nverts);
					
					if (!iterv1 || !iterv2)
						continue;

					/* check if vertex is tagged and has right valence */
					if(iterv1->flag || iterv2->flag) {
						if(uv_edge_tag_faces(em, iterv1, iterv2, &starttotf)) {
							looking= 1;
							BMO_SetFlag(em->bm, efa, EFA_F1_FLAG);

							uv_vertex_loop_flag(iterv1);
							uv_vertex_loop_flag(iterv2);
							break;
						}
					}
				}
			}
		}
	}

	/* do the actual select/deselect */
	nverts= hit->efa->len;
	iterv1= uv_vertex_map_get(vmap, hit->efa, hit->lindex);
	iterv2= uv_vertex_map_get(vmap, hit->efa, (hit->lindex+1)%nverts);
	iterv1->flag= 1;
	iterv2->flag= 1;

	if(extend) {
		if(uvedit_uv_selected(em, scene, hit->l) && uvedit_uv_selected(em, scene, hit->l))
			select= 0;
		else
			select= 1;
	}
	else
		select= 1;
	
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tf= CustomData_em_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

		a = 0;
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			iterv1= uv_vertex_map_get(vmap, efa, a);

			if(iterv1->flag) {
				if(select) uvedit_uv_select(em, scene, l);
				else uvedit_uv_deselect(em, scene, l);
			}

			a++;
		}
	}

	/* cleanup */
	EDBM_free_uv_vert_map(vmap);
	EDBM_free_index_arrays(em);

	return (select)? 1: -1;
}

/*********************** linked select ***********************/

static void select_linked(Scene *scene, Image *ima, BMEditMesh *em, float limit[2], NearestHit *hit, int extend)
{
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	UvVertMap *vmap;
	UvMapVert *vlist, *iterv, *startv;
	int a, i, nverts, j, stacksize= 0, *stack;
	char *flag;

	vmap= EDBM_make_uv_vert_map(em, 1, 1, limit);
	if(vmap == NULL)
		return;

	stack= MEM_mallocN(sizeof(*stack)*em->bm->totface, "UvLinkStack");
	flag= MEM_callocN(sizeof(*flag)*em->bm->totface, "UvLinkFlag");

	if(!hit) {
		a = 0;
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			if(uvedit_face_visible(scene, ima, efa, tf)) { 
				BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

					if (luv->flag & MLOOPUV_VERTSEL) {
						stack[stacksize]= a;
						stacksize++;
						flag[a]= 1;

						break;
					}
				}
			}
		}
		a++;
	}
	else {
		a = 0;
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if(efa == hit->efa) {
				stack[stacksize]= a;
				stacksize++;
				flag[a]= 1;
				break;
			}

			a++;
		}
	}

	while(stacksize > 0) {
		stacksize--;
		a= stack[stacksize];
		
		j = 0;
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if(j==a)
				break;

			j++;
		}

		nverts= efa->len;

		i = 0;
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {

			/* make_uv_vert_map_EM sets verts tmp.l to the indicies */
			vlist= EDBM_get_uv_map_vert(vmap, BMINDEX_GET(l->v));
			
			startv= vlist;

			for(iterv=vlist; iterv; iterv=iterv->next) {
				if(iterv->separate)
					startv= iterv;
				if(iterv->f == a)
					break;
			}

			for(iterv=startv; iterv; iterv=iterv->next) {
				if((startv != iterv) && (iterv->separate))
					break;
				else if(!flag[iterv->f]) {
					flag[iterv->f]= 1;
					stack[stacksize]= iterv->f;
					stacksize++;
				}
			}

			i++;
		}
	}

	if(!extend || hit) {		
		a = 0;
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				
				if (flag[a])
					luv->flag |= MLOOPUV_VERTSEL;
				else
					luv->flag &= ~MLOOPUV_VERTSEL;
			}
		}
	}
	else if(extend && hit) {
		a = 0;
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
						
				if (luv->flag & MLOOPUV_VERTSEL)
					break;
			}

			a++;
		}

		if(efa) {
			a = 0;
			BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
				if (!flag[a]) continue;

				BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					
					luv->flag &= ~MLOOPUV_VERTSEL;
				}

				a++;
			}
		}
		else {
			a = 0;
			BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
				if (!flag[a]) continue;

				BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					
					luv->flag |= MLOOPUV_VERTSEL;
				}

				a++;
			}
		}
	}
	
	MEM_freeN(stack);
	MEM_freeN(flag);
	EDBM_free_uv_vert_map(vmap);
}

/* ******************** align operator **************** */

static void weld_align_uv(bContext *C, int tool)
{
	Scene *scene;
	Object *obedit;
	Image *ima;
	BMEditMesh *em;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	float cent[2], min[2], max[2];
	
	scene= CTX_data_scene(C);
	obedit= CTX_data_edit_object(C);
	em= ((Mesh*)obedit->data)->edit_btmesh;
	ima= CTX_data_edit_image(C);

	INIT_MINMAX2(min, max);

	if(tool == 'a') {
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			if(!uvedit_face_visible(scene, ima, efa, tf))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				if (uvedit_uv_selected(em, scene, l)) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					DO_MINMAX2(luv->uv, min, max)
				}
			}
		}

		tool= (max[0]-min[0] >= max[1]-min[1])? 'y': 'x';
	}

	uvedit_center(scene, ima, obedit, cent, 0);

	if(tool == 'x' || tool == 'w') {
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if(!uvedit_face_visible(scene, ima, efa, tf))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				if (uvedit_uv_selected(em, scene, l)) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					luv->uv[0] = cent[0];
				}

			}
		}
	}

	if(tool == 'y' || tool == 'w') {
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if(!uvedit_face_visible(scene, ima, efa, tf))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				if (uvedit_uv_selected(em, scene, l)) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					luv->uv[1] = cent[1];
				}

			}
		}
	}

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);
}

static int align_exec(bContext *C, wmOperator *op)
{
	weld_align_uv(C, RNA_enum_get(op->ptr, "axis"));

	return OPERATOR_FINISHED;
}

void UV_OT_align(wmOperatorType *ot)
{
	static EnumPropertyItem axis_items[] = {
		{'a', "ALIGN_AUTO", 0, "Align Auto", "Automatically choose the axis on which there is most alignment already."},
		{'x', "ALIGN_X", 0, "Align X", "Align UVs on X axis."},
		{'y', "ALIGN_Y", 0, "Align Y", "Align UVs on Y axis."},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Align";
	ot->idname= "UV_OT_align";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= align_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_enum(ot->srna, "axis", axis_items, 'a', "Axis", "Axis to align UV locations on.");
}

/* ******************** weld operator **************** */

static int weld_exec(bContext *C, wmOperator *op)
{
	weld_align_uv(C, 'w');

	return OPERATOR_FINISHED;
}

void UV_OT_weld(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Weld";
	ot->idname= "UV_OT_weld";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= weld_exec;
	ot->poll= ED_operator_uvedit;
}

/* ******************** stitch operator **************** */

/* just for averaging UVs */
typedef struct UVVertAverage {
	float uv[2];
	int count;
} UVVertAverage;

static int stitch_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima;
	Scene *scene;
	Object *obedit;
	BMEditMesh *em;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	BMVert *eve;
	Image *ima;
	MTexPoly *tf;
	MLoopUV *luv;
	
	sima= CTX_wm_space_image(C);
	scene= CTX_data_scene(C);
	obedit= CTX_data_edit_object(C);
	em= ((Mesh*)obedit->data)->edit_btmesh;
	ima= CTX_data_edit_image(C);
	
	if(RNA_boolean_get(op->ptr, "use_limit")) {
		UvVertMap *vmap;
		UvMapVert *vlist, *iterv;
		float newuv[2], limit[2], pixels;
		int a, vtot;

		pixels= RNA_float_get(op->ptr, "limit");
		uvedit_pixel_to_float(sima, limit, pixels);

		EDBM_init_index_arrays(em, 0, 0, 1);
		vmap= EDBM_make_uv_vert_map(em, 1, 0, limit);

		if(vmap == NULL) {
			return OPERATOR_CANCELLED;
		}
		
		a = 0;
		BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			vlist= EM_get_uv_map_vert(vmap, a);

			while(vlist) {
				newuv[0]= 0; newuv[1]= 0;
				vtot= 0;

				for(iterv=vlist; iterv; iterv=iterv->next) {
					if((iterv != vlist) && iterv->separate)
						break;

					efa = EDBM_get_face_for_index(em, iterv->f);
					tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
					
					l = BMIter_AtIndex(em->bm, BM_LOOPS_OF_FACE, efa, iterv->tfindex);
					if (uvedit_uv_selected(em, scene, l)) {
						luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

						newuv[0] += luv->uv[0];
						newuv[1] += luv->uv[1];
						vtot++;
					}
				}

				if(vtot > 1) {
					newuv[0] /= vtot; newuv[1] /= vtot;

					for(iterv=vlist; iterv; iterv=iterv->next) {
						if((iterv != vlist) && iterv->separate)
							break;

						efa = EDBM_get_face_for_index(em, iterv->f);
						tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
						
						l = BMIter_AtIndex(em->bm, BM_LOOPS_OF_FACE, efa, iterv->tfindex);
						if (uvedit_uv_selected(em, scene, l)) {
							luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

							luv->uv[0] = newuv[0];
							luv->uv[1] = newuv[1];
							vtot++;
						}
					}
				}

				vlist= iterv;
			}

			a++;
		}

		EDBM_free_uv_vert_map(vmap);
		EDBM_free_index_arrays(em);
	}
	else {
		UVVertAverage *uv_average, *uvav;
		int count;

		// index and count verts
		count=0;
		BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			BMINDEX_SET(eve, count);
			count++;
		}
		
		uv_average= MEM_callocN(sizeof(UVVertAverage)*count, "Stitch");
		
		// gather uv averages per vert
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if(!uvedit_face_visible(scene, ima, efa, tf))
				continue;
			
			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				if(uvedit_uv_selected(em, scene, l)) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					uvav = uv_average + BMINDEX_GET(l->v);

					uvav->count++;
					uvav->uv[0] += luv->uv[0];
					uvav->uv[1] += luv->uv[1];
				}
			}
		}
		
		// apply uv welding
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if(!uvedit_face_visible(scene, ima, efa, tf))
				continue;
			
			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				if(uvedit_uv_selected(em, scene, l)) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					uvav = uv_average + BMINDEX_GET(l->v);
					luv->uv[0] = uvav->uv[0]/uvav->count;
					luv->uv[1] = uvav->uv[1]/uvav->count;
				}
			}
		}

		MEM_freeN(uv_average);
	}

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_stitch(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Stitch";
	ot->idname= "UV_OT_stitch";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= stitch_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "use_limit", 1, "Use Limit", "Stitch UVs within a specified limit distance.");
	RNA_def_float(ot->srna, "limit", 20.0, 0.0f, FLT_MAX, "Limit", "Limit distance in image pixels.", -FLT_MAX, FLT_MAX);
}

/* ******************** (de)select all operator **************** */

static int select_inverse_exec(bContext *C, wmOperator *op)
{
	Scene *scene;
	ToolSettings *ts;
	Object *obedit;
	BMEditMesh *em;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	Image *ima;
	MTexPoly *tf;
	MLoopUV *luv;
	
	scene= CTX_data_scene(C);
	ts= CTX_data_tool_settings(C);
	obedit= CTX_data_edit_object(C);
	em= ((Mesh*)obedit->data)->edit_btmesh;
	ima= CTX_data_edit_image(C);

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		EDBM_select_swap(em);
	}
	else {
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf = CustomData_em_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			if(!uvedit_face_visible(scene, ima, efa, tf))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				luv->flag = luv->flag ^ MLOOPUV_VERTSEL;
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_select_inverse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Inverse";
	ot->idname= "UV_OT_select_inverse";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= select_inverse_exec;
	ot->poll= ED_operator_uvedit;
}

/* ******************** (de)select all operator **************** */

static int de_select_all_exec(bContext *C, wmOperator *op)
{
	Scene *scene;
	ToolSettings *ts;
	Object *obedit;
	BMEditMesh *em;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	Image *ima;
	MTexPoly *tf;
	MLoopUV *luv;
	int sel = 1;
	
	scene= CTX_data_scene(C);
	ts= CTX_data_tool_settings(C);
	obedit= CTX_data_edit_object(C);
	em= ((Mesh*)obedit->data)->edit_btmesh;
	ima= CTX_data_edit_image(C);
	
	if(ts->uv_flag & UV_SYNC_SELECTION) {
		EDBM_toggle_select_all(((Mesh*)obedit->data)->edit_btmesh);
	}
	else {
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			if(!uvedit_face_visible(scene, ima, efa, tf))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

				if (luv->flag & MLOOPUV_VERTSEL) {
					sel= 0;
					break;
				}
			}
		}
	
		
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			if(!uvedit_face_visible(scene, ima, efa, tf))
				continue;

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

				if (sel) luv->flag |= MLOOPUV_VERTSEL;
				else luv->flag &= ~MLOOPUV_VERTSEL;
			}
		}
	}

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_select_all_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select or Deselect All";
	ot->idname= "UV_OT_select_all_toggle";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= de_select_all_exec;
	ot->poll= ED_operator_uvedit;
}

/* ******************** mouse select operator **************** */

static int sticky_select(float *limit, int hitv[4], int v, float *hituv[4], float *uv, int sticky, int hitlen)
{
	int i;

	/* this function test if some vertex needs to selected
	 * in addition to the existing ones due to sticky select */
	if(sticky == SI_STICKY_DISABLE)
		return 0;

	for(i=0; i<hitlen; i++) {
		if(hitv[i] == v) {
			if(sticky == SI_STICKY_LOC) {
				if(fabs(hituv[i][0]-uv[0]) < limit[0] && fabs(hituv[i][1]-uv[1]) < limit[1])
					return 1;
			}
			else if(sticky == SI_STICKY_VERTEX)
				return 1;
		}
	}

	return 0;
}

static int mouse_select(bContext *C, float co[2], int extend, int loop)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	NearestHit hit;
	int a, i, select = 1, selectmode, sticky, sync, *hitv=NULL, nvert;
	BLI_array_declare(hitv);
	int flush = 0, hitlen=0; /* 0 == dont flush, 1 == sel, -1 == desel;  only use when selection sync is enabled */
	float limit[2], **hituv = NULL;
	BLI_array_declare(hituv);
	float penalty[2];

	uvedit_pixel_to_float(sima, limit, 0.05f);
	uvedit_pixel_to_float(sima, penalty, 5.0f);

	/* retrieve operation mode */
	if(ts->uv_flag & UV_SYNC_SELECTION) {
		sync= 1;

		if(ts->selectmode & SCE_SELECT_FACE)
			selectmode= UV_SELECT_FACE;
		else if(ts->selectmode & SCE_SELECT_EDGE)
			selectmode= UV_SELECT_EDGE;
		else
			selectmode= UV_SELECT_VERTEX;

		sticky= SI_STICKY_DISABLE;
	}
	else {
		sync= 0;
		selectmode= ts->uv_selectmode;
		sticky= sima->sticky;
	}

	/* find nearest element */
	if(loop) {
		/* find edge */
		find_nearest_uv_edge(scene, ima, em, co, &hit);
		if(hit.efa == NULL) {
			return OPERATOR_CANCELLED;
		}

		hitlen = 0;
	}
	else if(selectmode == UV_SELECT_VERTEX) {
		/* find vertex */
		find_nearest_uv_vert(scene, ima, em, co, penalty, &hit);
		if(hit.efa == NULL) {
			return OPERATOR_CANCELLED;
		}

		/* mark 1 vertex as being hit */
		for(i=0; i<hit.efa->len; i++) {
			BLI_array_growone(hitv);
			BLI_array_growone(hituv);
			hitv[i]= 0xFFFFFFFF;
		}

		hitv[hit.lindex]= hit.vert1;
		hituv[hit.lindex]= hit.luv->uv;

		hitlen = hit.efa->len;
	}
	else if(selectmode == UV_SELECT_EDGE) {
		/* find edge */
		find_nearest_uv_edge(scene, ima, em, co, &hit);
		if(hit.efa == NULL) {
			return OPERATOR_CANCELLED;
		}

		/* mark 2 edge vertices as being hit */
		for(i=0; i<hit.efa->len; i++) {
			BLI_array_growone(hitv);
			BLI_array_growone(hituv);
			hitv[i]= 0xFFFFFFFF;
		}

		nvert= hit.efa->len;

		hitv[hit.lindex]= hit.vert1;
		hitv[(hit.lindex+1)%nvert]= hit.vert2;
		hituv[hit.lindex]= hit.luv->uv;
		hituv[(hit.lindex+1)%nvert]= hit.nextluv->uv;

		hitlen = hit.efa->len;
	}
	else if(selectmode == UV_SELECT_FACE) {
		/* find face */
		find_nearest_uv_face(scene, ima, em, co, &hit);
		if(hit.efa == NULL) {
			return OPERATOR_CANCELLED;
		}
		
		/* make active */
		EDBM_set_actFace(em, hit.efa);

		/* mark all face vertices as being hit */
		i = 0;
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, hit.efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

			BLI_array_growone(hitv);
			BLI_array_growone(hituv);
			hituv[i]= luv->uv;
			hitv[i] = BMINDEX_GET(l->v);
			i++;
		}
		
		hitlen = hit.efa->len;
	}
	else if(selectmode == UV_SELECT_ISLAND) {
		find_nearest_uv_vert(scene, ima, em, co, NULL, &hit);

		if(hit.efa==NULL) {
			return OPERATOR_CANCELLED;
		}

		hitlen = 0;
	}
	else {
		hitlen = 0;
		return OPERATOR_CANCELLED;
	}

	/* do selection */
	if(loop) {
		flush= select_edgeloop(scene, ima, em, &hit, limit, extend);
	}
	else if(selectmode == UV_SELECT_ISLAND) {
		select_linked(scene, ima, em, limit, &hit, extend);
	}
	else if(extend) {
		if(selectmode == UV_SELECT_VERTEX) {
			/* (de)select uv vertex */
			if(uvedit_uv_selected(em, scene, hit.l)) {
				uvedit_uv_deselect(em, scene, hit.l);
				select= 0;
			}
			else {
				uvedit_uv_select(em, scene, hit.l);
				select= 1;
			}
			flush = 1;
		}
		else if(selectmode == UV_SELECT_EDGE) {
			/* (de)select edge */
			if(uvedit_edge_selected(em, scene, hit.l)) {
				uvedit_edge_deselect(em, scene, hit.l);
				select= 0;
			}
			else {
				uvedit_edge_select(em, scene, hit.l);
				select= 1;
			}
			flush = 1;
		}
		else if(selectmode == UV_SELECT_FACE) {
			/* (de)select face */
			if(uvedit_face_selected(scene, em, hit.efa)) {
				uvedit_face_deselect(scene, em, hit.efa);
				select= 0;
			}
			else {
				uvedit_face_select(scene, em, hit.efa);
				select= 1;
			}
			flush = -1;
		}

		/* (de)select sticky uv nodes */
		if(sticky != SI_STICKY_DISABLE) {
			BMVert *ev;
			
			a = 0;
			BM_ITER(ev, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
				BMINDEX_SET(ev, a);
				a++;
			}

			/* deselect */
			if(select==0) {
				BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
					tf= CustomData_em_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
					if(!uvedit_face_visible(scene, ima, efa, tf))
						continue;

					BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
						luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
						if(sticky_select(limit, hitv, BMINDEX_GET(l->v), hituv, luv->uv, sticky, hitlen))
							uvedit_uv_deselect(em, scene, l);
					}
				}
				flush = -1;
			}
			/* select */
			else {
				BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
					tf= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
					if(!uvedit_face_visible(scene, ima, efa, tf))
						continue;

					BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
						luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
						if(sticky_select(limit, hitv, BMINDEX_GET(l->v), hituv, luv->uv, sticky, hitlen))
							uvedit_uv_select(em, scene, l);
					}
				}

				flush = 1;
			}			
		}
	}
	else {
		/* deselect all */
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			uvedit_face_deselect(scene, em, efa);
		}

		if(selectmode == UV_SELECT_VERTEX) {
			/* select vertex */
			uvedit_uv_select(em, scene, hit.l);
			flush= 1;
		}
		else if(selectmode == UV_SELECT_EDGE) {
			/* select edge */
			uvedit_edge_select(em, scene, hit.l);
			flush= 1;
		}
		else if(selectmode == UV_SELECT_FACE) {
			/* select face */
			uvedit_face_select(scene, em, hit.efa);
		}

		/* select sticky uvs */
		if(sticky != SI_STICKY_DISABLE) {
			BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
				tf= CustomData_em_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
				if(!uvedit_face_visible(scene, ima, efa, tf))
					continue;
				
				BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
					if(sticky == SI_STICKY_DISABLE) continue;
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

					if(sticky_select(limit, hitv, BMINDEX_GET(l->v), hituv, luv->uv, sticky, hitlen))
						uvedit_uv_select(em, scene, l);

					flush= 1;
				}
			}
		}
	}
#if 0 //bmesh does flushing through the BM_Select functions, so not sure
      //what to do about this bit.
	if(sync) {
		/* flush for mesh selection */
		if(ts->selectmode != SCE_SELECT_FACE) {
			if(flush==1)		EM_select_flush(em);
			else if(flush==-1)	EM_deselect_flush(em);
		}
	}
#endif	
	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
	
	return OPERATOR_PASS_THROUGH|OPERATOR_FINISHED;
}

static int select_exec(bContext *C, wmOperator *op)
{
	float co[2];
	int extend, loop;

	RNA_float_get_array(op->ptr, "location", co);
	extend= RNA_boolean_get(op->ptr, "extend");
	loop= 0;

	return mouse_select(C, co, extend, loop);
}

static int select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	float co[2];
	int x, y;

	x= event->x - ar->winrct.xmin;
	y= event->y - ar->winrct.ymin;

	UI_view2d_region_to_view(&ar->v2d, x, y, &co[0], &co[1]);
	RNA_float_set_array(op->ptr, "location", co);

	return select_exec(C, op);
}

void UV_OT_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select";
	ot->idname= "UV_OT_select";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= select_exec;
	ot->invoke= select_invoke;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
		"Extend", "Extend selection rather than clearing the existing selection.");
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
		"Location", "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds.", -100.0f, 100.0f);
}

/* ******************** loop select operator **************** */

static int select_loop_exec(bContext *C, wmOperator *op)
{
	float co[2];
	int extend, loop;

	RNA_float_get_array(op->ptr, "location", co);
	extend= RNA_boolean_get(op->ptr, "extend");
	loop= 1;

	return mouse_select(C, co, extend, loop);
}

static int select_loop_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	float co[2];
	int x, y;

	x= event->x - ar->winrct.xmin;
	y= event->y - ar->winrct.ymin;

	UI_view2d_region_to_view(&ar->v2d, x, y, &co[0], &co[1]);
	RNA_float_set_array(op->ptr, "location", co);

	return select_loop_exec(C, op);
}

void UV_OT_select_loop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Loop Select";
	ot->idname= "UV_OT_select_loop";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= select_loop_exec;
	ot->invoke= select_loop_invoke;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
		"Extend", "Extend selection rather than clearing the existing selection.");
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
		"Location", "Mouse location in normalized coordinates, 0.0 to 1.0 is within the image bounds.", -100.0f, 100.0f);
}

/* ******************** linked select operator **************** */

static int select_linked_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	float limit[2];
	int extend;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		BKE_report(op->reports, RPT_ERROR, "Can't select linked when sync selection is enabled.");
		return OPERATOR_CANCELLED;
	}

	extend= RNA_boolean_get(op->ptr, "extend");
	uvedit_pixel_to_float(sima, limit, 0.05f);
	select_linked(scene, ima, em, limit, NULL, extend);

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Linked";
	ot->idname= "UV_OT_select_linked";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= select_linked_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0,
		"Extend", "Extend selection rather than clearing the existing selection.");
}

/* ******************** unlink selection operator **************** */

static int unlink_selection_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		BKE_report(op->reports, RPT_ERROR, "Can't unlink selection when sync selection is enabled.");
		return OPERATOR_CANCELLED;
	}
	
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		int desel = 0;

		tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if(!uvedit_face_visible(scene, ima, efa, tf))
			continue;

		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->pdata, l->head.data, CD_MLOOPUV);
			
			if (!(luv->flag & MLOOPUV_VERTSEL)) {
				desel = 1;
				break;
			}
		}

		if (desel) {
			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->pdata, l->head.data, CD_MLOOPUV);
				luv->flag &= ~MLOOPUV_VERTSEL;
			}
		}
	}
	
	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_unlink_selection(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Unlink Selection";
	ot->idname= "UV_OT_unlink_selection";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= unlink_selection_exec;
	ot->poll= ED_operator_uvedit;
}

/* ******************** border select operator **************** */

/* This function sets the selection on tagged faces, need because settings the
 * selection a face is done in a number of places but it also needs to respect
 * the sticky modes for the UV verts, so dealing with the sticky modes is best
 * done in a seperate function.
 * 
 * De-selects faces that have been tagged on efa->tmp.l.  */

static void uv_faces_do_sticky(bContext *C, SpaceImage *sima, Scene *scene, Object *obedit, short select)
{
	/* Selecting UV Faces with some modes requires us to change 
	 * the selection in other faces (depending on the sticky mode).
	 * 
	 * This only needs to be done when the Mesh is not used for
	 * selection (so for sticky modes, vertex or location based). */
	
	ToolSettings *ts= CTX_data_tool_settings(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	
	if((ts->uv_flag & UV_SYNC_SELECTION)==0 && sima->sticky == SI_STICKY_VERTEX) {
		/* Tag all verts as untouched, then touch the ones that have a face center
		 * in the loop and select all MLoopUV's that use a touched vert. */
		BMVert *eve;
		
		BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL)
			BMINDEX_SET(eve, 0);
		
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if(BMINDEX_GET(efa)) {
				BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
					BMINDEX_SET(l->v, 1);
				}
			}
		}

		/* now select tagged verts */
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);

			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				if (BMINDEX_GET(l->v)) {
					if (select)
						uvedit_uv_select(em, scene, l);
					else
						uvedit_uv_deselect(em, scene, l);
				}
			}
		}
	}
	else if((ts->uv_flag & UV_SYNC_SELECTION)==0 && sima->sticky == SI_STICKY_LOC) {
		BMFace *efa_vlist;
		MTexPoly *tf_vlist;
		UvMapVert *start_vlist=NULL, *vlist_iter;
		struct UvVertMap *vmap;
		float limit[2];
		int efa_index;
		//BMVert *eve; /* removed vert counting for now */ 
		//int a;
		
		uvedit_pixel_to_float(sima, limit, 0.05);
		
		EDBM_init_index_arrays(em, 0, 0, 1);
		vmap= EDBM_make_uv_vert_map(em, 0, 0, limit);
		
		/* verts are numbered above in make_uv_vert_map_EM, make sure this stays true! */
		/*for(a=0, eve= em->verts.first; eve; a++, eve= eve->next)
			eve->tmp.l = a; */
		
		if(vmap == NULL) {
			return;
		}
		
		efa = BMIter_New(&iter, em->bm, BM_FACES_OF_MESH, NULL);
		for (efa_index=0; efa; efa=BMIter_Step(&iter), efa_index++) {
			if(BMINDEX_GET(efa)) {
				tf = CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
				
				BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
					if(select)
						uvedit_uv_select(em, scene, l);
					else
						uvedit_uv_deselect(em, scene, l);
					
					vlist_iter= EDBM_get_uv_map_vert(vmap, BMINDEX_GET(l->v));
					
					while (vlist_iter) {
						if(vlist_iter->separate)
							start_vlist = vlist_iter;
						
						if(efa_index == vlist_iter->f)
							break;

						vlist_iter = vlist_iter->next;
					}
				
					vlist_iter = start_vlist;
					while (vlist_iter) {
						
						if(vlist_iter != start_vlist && vlist_iter->separate)
							break;
						
						if(efa_index != vlist_iter->f) {
							efa_vlist = EDBM_get_face_for_index(em, vlist_iter->f);
							tf_vlist = CustomData_bmesh_get(&em->bm->pdata, efa_vlist->head.data, CD_MTEXPOLY);
							
							if(select)
								uvedit_uv_select(em, scene, BMIter_AtIndex(em->bm, BM_LOOPS_OF_FACE, efa_vlist, vlist_iter->tfindex));
							else
								uvedit_uv_deselect(em, scene, BMIter_AtIndex(em->bm, BM_LOOPS_OF_FACE, efa_vlist, vlist_iter->tfindex));
						}
						vlist_iter = vlist_iter->next;
					}
				}
			}
		}
		EDBM_free_index_arrays(em);
		EDBM_free_uv_vert_map(vmap);
		
	}
	else { /* SI_STICKY_DISABLE or ts->uv_flag & UV_SYNC_SELECTION */
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if(BMINDEX_GET(efa)) {
				if(select)
					uvedit_face_select(scene, em, efa);
				else
					uvedit_face_deselect(scene, em, efa);
			}
		}
	}
}

static int border_select_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	ARegion *ar= CTX_wm_region(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	rcti rect;
	rctf rectf;
	int change, pinned, select, faces;

	/* get rectangle from operator */
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
		
	UI_view2d_region_to_view(&ar->v2d, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(&ar->v2d, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);

	/* figure out what to select/deselect */
	select= (RNA_int_get(op->ptr, "event_type") == LEFTMOUSE); // XXX hardcoded
	pinned= RNA_boolean_get(op->ptr, "pinned");
	
	if(ts->uv_flag & UV_SYNC_SELECTION)
		faces= (ts->selectmode == SCE_SELECT_FACE);
	else
		faces= (ts->uv_selectmode == UV_SELECT_FACE);

	/* do actual selection */
	if(faces && !pinned) {
		/* handle face selection mode */
		float cent[2];

		change= 0;

		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			/* assume not touched */
			BMINDEX_SET(efa, 0);

			tf= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if(uvedit_face_visible(scene, ima, efa, tf)) {
				poly_uv_center(em, efa, cent);
				if(BLI_in_rctf(&rectf, cent[0], cent[1])) {
					BMINDEX_SET(efa, 1);
					change = 1;
				}
			}
		}

		/* (de)selects all tagged faces and deals with sticky modes */
		if(change)
			uv_faces_do_sticky(C, sima, scene, obedit, select);
	}
	else {
		/* other selection modes */
		change= 1;
		
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			tf= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
			if(!uvedit_face_visible(scene, ima, efa, tf))
				continue;
			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

				if(!pinned || (ts->uv_flag & UV_SYNC_SELECTION) ) {

					/* UV_SYNC_SELECTION - can't do pinned selection */
					if(BLI_in_rctf(&rectf, luv->uv[0], luv->uv[1])) {
						if(select)	uvedit_uv_select(em, scene, l);
						else		uvedit_uv_deselect(em, scene, l);
					}
				} else if(pinned) {
					if ((luv->flag & MLOOPUV_PINNED) && 
					    BLI_in_rctf(&rectf, luv->uv[0], luv->uv[1])) {
						if(select)	uvedit_uv_select(em, scene, l);
						else		uvedit_uv_deselect(em, scene, l);
					}
				}
			}
		}
	}

	if(change) {
		/* make sure newly selected vert selection is updated*/
#if 0 //ok, I think the BM_Select API handles all of this?
		if(ts->uv_flag & UV_SYNC_SELECTION) {
			if(ts->selectmode != SCE_SELECT_FACE) {
				if(select)	EM_select_flush(em);
				else		EM_deselect_flush(em);
			}
		}
#endif

		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
		
		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
} 

void UV_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->idname= "UV_OT_select_border";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= border_select_exec;
	ot->modal= WM_border_select_modal;
	ot->poll= ED_operator_uvedit;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "pinned", 0, "Pinned", "Border select pinned UVs only.");

	RNA_def_int(ot->srna, "event_type", 0, INT_MIN, INT_MAX, "Event Type", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmin", 0, INT_MIN, INT_MAX, "X Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmax", 0, INT_MIN, INT_MAX, "X Max", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymin", 0, INT_MIN, INT_MAX, "Y Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymax", 0, INT_MIN, INT_MAX, "Y Max", "", INT_MIN, INT_MAX);
}

/* ******************** circle select operator **************** */

static void select_uv_inside_ellipse(BMEditMesh *em, SpaceImage *sima, 
				     Scene *scene, int select, float *offset,
				     float *ell, BMLoop *l, MLoopUV *luv)
{
	/* normalized ellipse: ell[0] = scaleX, ell[1] = scaleY */
	float x, y, r2, *uv;
	
	
	uv= luv->uv;

	x= (uv[0] - offset[0])*ell[0];
	y= (uv[1] - offset[1])*ell[1];

	r2 = x*x + y*y;
	if(r2 < 1.0) {
		if(select) uvedit_uv_select(em, scene, l);
		else uvedit_uv_deselect(em, scene, l);
	}
}

int circle_select_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	ARegion *ar= CTX_wm_region(C);
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MLoopUV *luv;
	MTexPoly *tface;
	int x, y, radius, width, height, select;
	float zoomx, zoomy, offset[2], ellipse[2];

	/* get operator properties */
	select= (RNA_int_get(op->ptr, "event_type") == LEFTMOUSE); // XXX hardcoded
	x= RNA_int_get(op->ptr, "x");
	y= RNA_int_get(op->ptr, "y");
	radius= RNA_int_get(op->ptr, "radius");

	/* compute ellipse size and location, not a circle since we deal
	 * with non square image. ellipse is normalized, r = 1.0. */
	ED_space_image_size(sima, &width, &height);
	ED_space_image_zoom(sima, ar, &zoomx, &zoomy);

	ellipse[0]= width*zoomx/radius;
	ellipse[1]= height*zoomy/radius;

	UI_view2d_region_to_view(&ar->v2d, x, y, &offset[0], &offset[1]);
	
	/* do selection */
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			select_uv_inside_ellipse(em, sima, scene, select, offset, ellipse, l, luv);
		}
	}

#if 0 //I think the BM_Select api stuff handles all this as necassary?
	if(select) EM_select_flush(em);
	else EM_deselect_flush(em);
#endif
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_circle_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Circle Select";
	ot->idname= "UV_OT_circle_select";
	
	/* api callbacks */
	ot->invoke= WM_gesture_circle_invoke;
	ot->modal= WM_gesture_circle_modal;
	ot->exec= circle_select_exec;
	ot->poll= ED_operator_uvedit;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "radius", 0, INT_MIN, INT_MAX, "Radius", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "event_type", 0, INT_MIN, INT_MAX, "Event Type", "", INT_MIN, INT_MAX);
}

/* ******************** snap cursor operator **************** */

static void snap_uv_to_pixel(float *uvco, float w, float h)
{
	uvco[0] = ((float)((int)((uvco[0]*w) + 0.5f)))/w;
	uvco[1] = ((float)((int)((uvco[1]*h) + 0.5f)))/h;
}

static void snap_cursor_to_pixels(SpaceImage *sima, View2D *v2d)
{
	int width= 0, height= 0;

	ED_space_image_size(sima, &width, &height);
	snap_uv_to_pixel(v2d->cursor, width, height);
}

static int snap_cursor_to_selection(Scene *scene, Image *ima, Object *obedit, View2D *v2d)
{
	return uvedit_center(scene, ima, obedit, v2d->cursor, 0);
}

static int snap_cursor_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	ARegion *ar= CTX_wm_region(C);
	int change= 0;

	switch(RNA_boolean_get(op->ptr, "target")) {
		case 0:
			snap_cursor_to_pixels(sima, &ar->v2d);
			change= 1;
			break;
		case 1:
			change= snap_cursor_to_selection(scene, ima, obedit, &ar->v2d);
			break;
	}

	if(!change)
		return OPERATOR_CANCELLED;
	
	ED_region_tag_redraw(ar);

	return OPERATOR_FINISHED;
}

void UV_OT_snap_cursor(wmOperatorType *ot)
{
	static EnumPropertyItem target_items[] = {
		{0, "PIXELS", 0, "Pixels", ""},
		{1, "SELECTION", 0, "Selection", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Snap Cursor";
	ot->idname= "UV_OT_snap_cursor";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= snap_cursor_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_enum(ot->srna, "target", target_items, 0, "Target", "Target to snap the selected UV's to.");
}

/* ******************** snap selection operator **************** */

static int snap_uvs_to_cursor(Scene *scene, Image *ima, Object *obedit, View2D *v2d)
{
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tface;
	MLoopUV *luv;
	short change= 0;

	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tface= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if(!uvedit_face_visible(scene, ima, efa, tface))
			continue;

		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			if(uvedit_uv_selected(em, scene, l)) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				VECCOPY2D(luv->uv, v2d->cursor);
				change= 1;
			}
		}
	}

	return change;
}

static int snap_uvs_to_adjacent_unselected(Scene *scene, Image *ima, Object *obedit)
{
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	BMVert *eve;
	MTexPoly *tface;
	MLoopUV *luv;
	short change = 0;
	int count = 0;
	float *coords;
	short *usercount, users;
	
	/* set all verts to -1 : an unused index*/
	BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL)
		BMINDEX_SET(eve, -1);
	
	/* index every vert that has a selected UV using it, but only once so as to
	 * get unique indicies and to count how much to malloc */
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tface= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if(!uvedit_face_visible(scene, ima, efa, tface)) {
			BMINDEX_SET(efa, 0);
			continue;
		} else {
			BMINDEX_SET(efa, 1);
		}

		change = 1;
		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			if (uvedit_uv_selected(em, scene, l) && BMINDEX_GET(l->v) == -1) {
				BMINDEX_SET(l->v, count);
				count++;
			}
		}
	}
	
	coords = MEM_callocN(sizeof(float)*count*2, "snap to adjacent coords");
	usercount = MEM_callocN(sizeof(short)*count, "snap to adjacent counts");
	
	/* add all UV coords from visible, unselected UV coords as well as counting them to average later */
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if (!BMINDEX_GET(efa))
			continue;

		tface= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if(!uvedit_face_visible(scene, ima, efa, tface))
			continue;

		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			if (BMINDEX_GET(l->v) >= 0 && 
			    (!uvedit_uv_selected(em, scene, l))) {
				    luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				    coords[BMINDEX_GET(l->v)*2] += luv->uv[0];
				    coords[BMINDEX_GET(l->v)*2+1] += luv->uv[1];
				    change = 1;
			}
		}
	}
	
	/* no other verts selected, bail out */
	if(!change) {
		MEM_freeN(coords);
		MEM_freeN(usercount);
		return change;
	}
	
	/* copy the averaged unselected UVs back to the selected UVs */
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if (!BMINDEX_GET(efa))
			continue;

		tface= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if(!uvedit_face_visible(scene, ima, efa, tface))
			continue;

		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			if (uvedit_uv_selected(em, scene, l) && BMINDEX_GET(l->v) >= 0
			    && (users = usercount[BMINDEX_GET(l->v)])) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				luv->uv[0] = coords[BMINDEX_GET(l->v)*2];
				luv->uv[1] = coords[BMINDEX_GET(l->v)*2+1];
			}
		}
	}
	
	MEM_freeN(coords);
	MEM_freeN(usercount);

	return change;
}

static int snap_uvs_to_pixels(SpaceImage *sima, Scene *scene, Object *obedit)
{
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	Image *ima= sima->image;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tface;
	MLoopUV *luv;
	int width= 0, height= 0;
	float w, h;
	short change = 0;

	ED_space_image_size(sima, &width, &height);
	w = (float)width;
	h = (float)height;
	
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tface= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if(!uvedit_face_visible(scene, ima, efa, tface))
			continue;

		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			if (uvedit_uv_selected(em, scene, l)) {
				luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
				snap_uv_to_pixel(luv->uv, w, h);
			}
		}

		change = 1;
	}

	return change;
}

static int snap_selection_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	ARegion *ar= CTX_wm_region(C);
	int change= 0;

	switch(RNA_boolean_get(op->ptr, "target")) {
		case 0:
			change= snap_uvs_to_pixels(sima, scene, obedit);
			break;
		case 1:
			change= snap_uvs_to_cursor(scene, ima, obedit, &ar->v2d);
			break;
		case 2:
			change= snap_uvs_to_adjacent_unselected(scene, ima, obedit);
			break;
	}

	if(!change)
		return OPERATOR_CANCELLED;
	
	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_snap_selection(wmOperatorType *ot)
{
	static EnumPropertyItem target_items[] = {
		{0, "PIXELS", 0, "Pixels", ""},
		{1, "CURSOR", 0, "Cursor", ""},
		{2, "ADJACENT_UNSELECTED", 0, "Adjacent Unselected", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Snap Selection";
	ot->idname= "UV_OT_snap_selection";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= snap_selection_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_enum(ot->srna, "target", target_items, 0, "Target", "Target to snap the selected UV's to.");
}

/* ******************** pin operator **************** */

static int pin_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tface;
	MLoopUV *luv;
	int clear= RNA_boolean_get(op->ptr, "clear");
	
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tface= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if(!uvedit_face_visible(scene, ima, efa, tface))
			continue;

		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			
			if(!clear) {
				if (uvedit_uv_selected(em, scene, l))
					luv->flag |= MLOOPUV_PINNED;
			} else {
				if (uvedit_uv_selected(em, scene, l))
					luv->flag &= ~MLOOPUV_PINNED;
			}
		}
	}
	
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_pin(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Pin";
	ot->idname= "UV_OT_pin";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= pin_exec;
	ot->poll= ED_operator_uvedit;

	/* properties */
	RNA_def_boolean(ot->srna, "clear", 0, "Clear", "Clear pinning for the selection instead of setting it.");
}

/******************* select pinned operator ***************/

static int select_pinned_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Image *ima= CTX_data_edit_image(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tface;
	MLoopUV *luv;
	
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		tface= CustomData_bmesh_get(&em->bm->pdata, efa->head.data, CD_MTEXPOLY);
		if(!uvedit_face_visible(scene, ima, efa, tface))
			continue;

		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
			
			if (luv->flag & MLOOPUV_PINNED)
				uvedit_uv_select(em, scene, l);
		}
	}
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_select_pinned(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Selected Pinned";
	ot->idname= "UV_OT_select_pinned";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= select_pinned_exec;
	ot->poll= ED_operator_uvedit;
}

/********************** hide operator *********************/

static int hide_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	int swap= RNA_boolean_get(op->ptr, "unselected");

	if(ts->uv_flag & UV_SYNC_SELECTION) {
		EDBM_hide_mesh(em, swap);
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

		return OPERATOR_FINISHED;
	}
	
	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		int hide = 0;

		BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
			luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);

			if (luv->flag & MLOOPUV_VERTSEL) {
				hide = 1;
				break;
			}
		}

		if (swap)
			hide = !hide;
		
		if (hide) {
			BM_Select(em->bm, efa, 0);
			uvedit_face_deselect(scene, em, efa);
		}
	}
	
	///*deselects too many but ok for now*/
	//if(em->selectmode & (SCE_SELECT_EDGE|SCE_SELECT_VERTEX))
	//	EM_deselect_flush(em);
	
	
	EDBM_validate_selections(em);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Hide Selected";
	ot->idname= "UV_OT_hide";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= hide_exec;
	ot->poll= ED_operator_uvedit;

	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected.");
}

/****************** reveal operator ******************/

static int reveal_exec(bContext *C, wmOperator *op)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	BMFace *efa;
	BMLoop *l;
	BMVert *v;
	BMIter iter, liter;
	MTexPoly *tf;
	MLoopUV *luv;
	
	BM_ITER(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
		BMINDEX_SET(v, BM_TestHFlag(v, BM_SELECT));
	}

	/* call the mesh function if we are in mesh sync sel */
	if(ts->uv_flag & UV_SYNC_SELECTION) {
		EDBM_reveal_mesh(em);
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

		return OPERATOR_FINISHED;
	}

	BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
		if (em->selectmode == SCE_SELECT_FACE) {
			uvedit_face_select(scene, em, efa);
		} else {
			BM_ITER(l, &liter, em->bm, BM_LOOPS_OF_FACE, efa) {
				if (!BMINDEX_GET(l->v)) {
					luv = CustomData_bmesh_get(&em->bm->ldata, l->head.data, CD_MLOOPUV);
					luv->flag |= MLOOPUV_VERTSEL;
				}
			}
		}

		BM_Select(em->bm, efa, 1);
	}

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void UV_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reveal Hidden";
	ot->idname= "UV_OT_reveal";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* api callbacks */
	ot->exec= reveal_exec;
	ot->poll= ED_operator_uvedit;
}

/******************** set 3d cursor operator ********************/

static int set_2d_cursor_exec(bContext *C, wmOperator *op)
{
	ARegion *ar= CTX_wm_region(C);
	float location[2];

	RNA_float_get_array(op->ptr, "location", location);
	ar->v2d.cursor[0]= location[0];
	ar->v2d.cursor[1]= location[1];
	
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

static int set_2d_cursor_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar= CTX_wm_region(C);
	int x, y;
	float location[2];

	x= event->x - ar->winrct.xmin;
	y= event->y - ar->winrct.ymin;
	UI_view2d_region_to_view(&ar->v2d, x, y, &location[0], &location[1]);
	RNA_float_set_array(op->ptr, "location", location);

	return set_2d_cursor_exec(C, op);
}

void UV_OT_cursor_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set 3D Cursor";
	ot->idname= "UV_OT_cursor_set";
	
	/* api callbacks */
	ot->exec= set_2d_cursor_exec;
	ot->invoke= set_2d_cursor_invoke;
	ot->poll= ED_operator_uvedit;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX, "Location", "Cursor location in 0.0-1.0 coordinates.", -10.0f, 10.0f);
}

/********************** set tile operator **********************/

static int set_tile_exec(bContext *C, wmOperator *op)
{
	Image *ima= CTX_data_edit_image(C);
	int tile[2];

	if(!ima || !(ima->tpageflag & IMA_TILES))
		return OPERATOR_CANCELLED;

	RNA_int_get_array(op->ptr, "tile", tile);
	ED_uvedit_set_tile(C, CTX_data_scene(C), CTX_data_edit_object(C), ima, tile[0] + ima->xrep*tile[1]);

	ED_area_tag_redraw(CTX_wm_area(C));

	return OPERATOR_FINISHED;
}

static int set_tile_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	SpaceImage *sima= CTX_wm_space_image(C);
	Image *ima= CTX_data_edit_image(C);
	ARegion *ar= CTX_wm_region(C);
	float fx, fy;
	int x, y, tile[2];

	if(!ima || !(ima->tpageflag & IMA_TILES))
		return OPERATOR_CANCELLED;

	x= event->x - ar->winrct.xmin;
	y= event->y - ar->winrct.ymin;
	UI_view2d_region_to_view(&ar->v2d, x, y, &fx, &fy);

	if(fx>=0.0 && fy>=0.0 && fx<1.0 && fy<1.0) {
		fx= fx*ima->xrep;
		fy= fy*ima->yrep;
		
		tile[0]= fx;
		tile[1]= fy;
		
		sima->curtile= tile[1]*ima->xrep + tile[0];
		RNA_int_set_array(op->ptr, "tile", tile);
	}

	return set_tile_exec(C, op);
}

void UV_OT_tile_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Tile";
	ot->idname= "UV_OT_tile_set";
	
	/* api callbacks */
	ot->exec= set_tile_exec;
	ot->invoke= set_tile_invoke;
	ot->poll= ED_operator_uvedit;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	RNA_def_int_vector(ot->srna, "tile", 2, NULL, 0, INT_MAX, "Tile", "Tile coordinate.", 0, 10);
}

/* ************************** registration **********************************/

void ED_operatortypes_uvedit(void)
{
	WM_operatortype_append(UV_OT_select_all_toggle);
	WM_operatortype_append(UV_OT_select_inverse);
	WM_operatortype_append(UV_OT_select);
	WM_operatortype_append(UV_OT_select_loop);
	WM_operatortype_append(UV_OT_select_linked);
	WM_operatortype_append(UV_OT_unlink_selection);
	WM_operatortype_append(UV_OT_select_pinned);
	WM_operatortype_append(UV_OT_select_border);
	WM_operatortype_append(UV_OT_circle_select);

	WM_operatortype_append(UV_OT_snap_cursor);
	WM_operatortype_append(UV_OT_snap_selection);

	WM_operatortype_append(UV_OT_align);
	WM_operatortype_append(UV_OT_stitch);
	WM_operatortype_append(UV_OT_weld);
	WM_operatortype_append(UV_OT_pin);

	WM_operatortype_append(UV_OT_average_islands_scale);
	WM_operatortype_append(UV_OT_cube_project);
	WM_operatortype_append(UV_OT_cylinder_project);
	WM_operatortype_append(UV_OT_from_view);
	WM_operatortype_append(UV_OT_mapping_menu);
	WM_operatortype_append(UV_OT_minimize_stretch);
	WM_operatortype_append(UV_OT_pack_islands);
	WM_operatortype_append(UV_OT_reset);
	WM_operatortype_append(UV_OT_sphere_project);
	WM_operatortype_append(UV_OT_unwrap);

	WM_operatortype_append(UV_OT_reveal);
	WM_operatortype_append(UV_OT_hide);

	WM_operatortype_append(UV_OT_cursor_set);
	WM_operatortype_append(UV_OT_tile_set);
}

void ED_keymap_uvedit(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	
	keymap= WM_keymap_find(keyconf, "UVEdit", 0, 0);
	keymap->poll= ED_operator_uvedit;
	
	/* pick selection */
	WM_keymap_add_item(keymap, "UV_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend", 1);
	WM_keymap_add_item(keymap, "UV_OT_select_loop", SELECTMOUSE, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_loop", SELECTMOUSE, KM_PRESS, KM_SHIFT|KM_ALT, 0)->ptr, "extend", 1);

	/* border/circle selection */
	WM_keymap_add_item(keymap, "UV_OT_select_border", BKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_select_border", BKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "pinned", 1);
	WM_keymap_add_item(keymap, "UV_OT_circle_select", CKEY, KM_PRESS, 0, 0);

	/* selection manipulation */
	WM_keymap_add_item(keymap, "UV_OT_select_linked", LKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "UV_OT_unlink_selection", LKEY, KM_PRESS, KM_ALT, 0);
	WM_keymap_add_item(keymap, "UV_OT_select_all_toggle", AKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UV_OT_select_inverse", IKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "UV_OT_select_pinned", PKEY, KM_PRESS, KM_SHIFT, 0);

	/* uv operations */
	WM_keymap_add_item(keymap, "UV_OT_stitch", VKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UV_OT_pin", PKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_pin", PKEY, KM_PRESS, KM_ALT, 0)->ptr, "clear", 1);

	/* unwrap */
	WM_keymap_add_item(keymap, "UV_OT_unwrap", EKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UV_OT_minimize_stretch", VKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "UV_OT_pack_islands", PKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "UV_OT_average_islands_scale", AKEY, KM_PRESS, KM_CTRL, 0);

	/* hide */
	WM_keymap_add_item(keymap, "UV_OT_hide", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "UV_OT_hide", HKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "unselected", 1);
	WM_keymap_add_item(keymap, "UV_OT_reveal", HKEY, KM_PRESS, KM_ALT, 0);

	/* cursor */
	WM_keymap_add_item(keymap, "UV_OT_cursor_set", ACTIONMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "UV_OT_tile_set", ACTIONMOUSE, KM_PRESS, KM_SHIFT, 0);

	ED_object_generic_keymap(keyconf, keymap, TRUE);

	transform_keymap_for_space(keyconf, keymap, SPACE_IMAGE);
}

