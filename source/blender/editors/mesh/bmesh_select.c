/**
 * $Id: 
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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*

BMEditMesh_mods.c, UI level access, no geometry changes 

*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "MTC_matrixops.h"

#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_context.h"
#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "RE_render_ext.h"  /* externtex */

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "bmesh.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "mesh_intern.h"

#include "BLO_sys_types.h" // for intptr_t support

/* XXX */
static void waitcursor() {}
static int pupmenu() {return 0;}

/* ****************************** MIRROR **************** */

void EDBM_select_mirrored(Object *obedit, BMEditMesh *em)
{
#if 0 //BMESH_TODO
	if(em->selectmode & SCE_SELECT_VERTEX) {
		BMVert *eve, *v1;
		
		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->f & SELECT) {
				v1= BMEditMesh_get_x_mirror_vert(obedit, em, eve->co);
				if(v1) {
					eve->f &= ~SELECT;
					v1->f |= SELECT;
				}
			}
		}
	}
#endif
}

void EDBM_automerge(int update) 
{
// XXX	int len;
	
//	if ((scene->automerge) &&
//		(obedit && obedit->type==OB_MESH) &&
//		(((Mesh*)obedit->data)->mr==NULL)
//	  ) {
//		len = removedoublesflag(1, 1, scene->toolsettings->doublimit);
//		if (len) {
//			em->totvert -= len; /* saves doing a countall */
//			if (update) {
//				DAG_object_flush_update(scene, obedit, OB_RECALC_DATA);
//			}
//		}
//	}
}

/* ****************************** SELECTION ROUTINES **************** */

unsigned int bm_solidoffs=0, bm_wireoffs=0, bm_vertoffs=0;	/* set in drawobject.c ... for colorindices */

/* facilities for border select and circle select */
static char *selbuf= NULL;

/* opengl doesn't support concave... */
static void draw_triangulated(short mcords[][2], short tot)
{
	ListBase lb={NULL, NULL};
	DispList *dl;
	float *fp;
	int a;
	
	/* make displist */
	dl= MEM_callocN(sizeof(DispList), "poly disp");
	dl->type= DL_POLY;
	dl->parts= 1;
	dl->nr= tot;
	dl->verts= fp=  MEM_callocN(tot*3*sizeof(float), "poly verts");
	BLI_addtail(&lb, dl);
	
	for(a=0; a<tot; a++, fp+=3) {
		fp[0]= (float)mcords[a][0];
		fp[1]= (float)mcords[a][1];
	}
	
	/* do the fill */
	filldisplist(&lb, &lb);

	/* do the draw */
	dl= lb.first;	/* filldisplist adds in head of list */
	if(dl->type==DL_INDEX3) {
		int *index;
		
		a= dl->parts;
		fp= dl->verts;
		index= dl->index;
		glBegin(GL_TRIANGLES);
		while(a--) {
			glVertex3fv(fp+3*index[0]);
			glVertex3fv(fp+3*index[1]);
			glVertex3fv(fp+3*index[2]);
			index+= 3;
		}
		glEnd();
	}
	
	freedisplist(&lb);
}


/* reads rect, and builds selection array for quick lookup */
/* returns if all is OK */
int EDBM_init_backbuf_border(ViewContext *vc, short xmin, short ymin, short xmax, short ymax)
{
	struct ImBuf *buf;
	unsigned int *dr;
	int a;
	
	if(vc->obedit==NULL || vc->v3d->drawtype<OB_SOLID || (vc->v3d->flag & V3D_ZBUF_SELECT)==0) return 0;
	
	buf= view3d_read_backbuf(vc, xmin, ymin, xmax, ymax);
	if(buf==NULL) return 0;
	if(bm_vertoffs==0) return 0;

	dr = buf->rect;
	
	/* build selection lookup */
	selbuf= MEM_callocN(bm_vertoffs+1, "selbuf");
	
	a= (xmax-xmin+1)*(ymax-ymin+1);
	while(a--) {
		if(*dr>0 && *dr<=bm_vertoffs) 
			selbuf[*dr]= 1;
		dr++;
	}
	IMB_freeImBuf(buf);
	return 1;
}

int EDBM_check_backbuf(unsigned int index)
{
	if(selbuf==NULL) return 1;
	if(index>0 && index<=bm_vertoffs)
		return selbuf[index];
	return 0;
}

void EDBM_free_backbuf(void)
{
	if(selbuf) MEM_freeN(selbuf);
	selbuf= NULL;
}

/* mcords is a polygon mask
   - grab backbuffer,
   - draw with black in backbuffer, 
   - grab again and compare
   returns 'OK' 
*/
int EDBM_mask_init_backbuf_border(ViewContext *vc, short mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax)
{
	unsigned int *dr, *drm;
	struct ImBuf *buf, *bufmask;
	int a;
	
	/* method in use for face selecting too */
	if(vc->obedit==NULL) {
		if(paint_facesel_test(vc->obact));
		else return 0;
	}
	else if(vc->v3d->drawtype<OB_SOLID || (vc->v3d->flag & V3D_ZBUF_SELECT)==0) return 0;

	buf= view3d_read_backbuf(vc, xmin, ymin, xmax, ymax);
	if(buf==NULL) return 0;
	if(bm_vertoffs==0) return 0;

	dr = buf->rect;

	/* draw the mask */
	glDisable(GL_DEPTH_TEST);
	
	glColor3ub(0, 0, 0);
	
	/* yah, opengl doesn't do concave... tsk! */
	ED_region_pixelspace(vc->ar);
 	draw_triangulated(mcords, tot);	
	
	glBegin(GL_LINE_LOOP);	/* for zero sized masks, lines */
	for(a=0; a<tot; a++) glVertex2s(mcords[a][0], mcords[a][1]);
	glEnd();
	
	glFinish();	/* to be sure readpixels sees mask */
	
	/* grab mask */
	bufmask= view3d_read_backbuf(vc, xmin, ymin, xmax, ymax);
	drm = bufmask->rect;
	if(bufmask==NULL) return 0; /* only when mem alloc fails, go crash somewhere else! */
	
	/* build selection lookup */
	selbuf= MEM_callocN(bm_vertoffs+1, "selbuf");
	
	a= (xmax-xmin+1)*(ymax-ymin+1);
	while(a--) {
		if(*dr>0 && *dr<=bm_vertoffs && *drm==0) selbuf[*dr]= 1;
		dr++; drm++;
	}
	IMB_freeImBuf(buf);
	IMB_freeImBuf(bufmask);
	return 1;
	
}

/* circle shaped sample area */
int EDBM_init_backbuf_circle(ViewContext *vc, short xs, short ys, short rads)
{
	struct ImBuf *buf;
	unsigned int *dr;
	short xmin, ymin, xmax, ymax, xc, yc;
	int radsq;
	
	/* method in use for face selecting too */
	if(vc->obedit==NULL) {
		if(paint_facesel_test(vc->obact));
		else return 0;
	}
	else if(vc->v3d->drawtype<OB_SOLID || (vc->v3d->flag & V3D_ZBUF_SELECT)==0) return 0;
	
	xmin= xs-rads; xmax= xs+rads;
	ymin= ys-rads; ymax= ys+rads;
	buf= view3d_read_backbuf(vc, xmin, ymin, xmax, ymax);
	if(bm_vertoffs==0) return 0;
	if(buf==NULL) return 0;

	dr = buf->rect;
	
	/* build selection lookup */
	selbuf= MEM_callocN(bm_vertoffs+1, "selbuf");
	radsq= rads*rads;
	for(yc= -rads; yc<=rads; yc++) {
		for(xc= -rads; xc<=rads; xc++, dr++) {
			if(xc*xc + yc*yc < radsq) {
				if(*dr>0 && *dr<=bm_vertoffs) selbuf[*dr]= 1;
			}
		}
	}

	IMB_freeImBuf(buf);
	return 1;
	
}

static void findnearestvert__doClosest(void *userData, BMVert *eve, int x, int y, int index)
{
	struct { short mval[2], pass, select, strict; int dist, lastIndex, closestIndex; BMVert *closest; } *data = userData;

	if (data->pass==0) {
		if (index<=data->lastIndex)
			return;
	} else {
		if (index>data->lastIndex)
			return;
	}

	if (data->dist>3) {
		int temp = abs(data->mval[0] - x) + abs(data->mval[1]- y);
		if (BM_TestHFlag(eve, BM_SELECT) == data->select) {
			if (data->strict == 1)
				return;
			else
				temp += 5;
		}

		if (temp<data->dist) {
			data->dist = temp;
			data->closest = eve;
			data->closestIndex = index;
		}
	}
}




static unsigned int findnearestvert__backbufIndextest(void *handle, unsigned int index)
{
	BMEditMesh *em= (BMEditMesh *)handle;
	BMIter iter;
	BMVert *eve = BMIter_AtIndex(em->bm, BM_VERTS_OF_MESH, NULL, index-1);

	if(eve && BM_TestHFlag(eve, BM_SELECT)) return 0;
	return 1; 
}
/**
 * findnearestvert
 * 
 * dist (in/out): minimal distance to the nearest and at the end, actual distance
 * sel: selection bias
 * 		if SELECT, selected vertice are given a 5 pixel bias to make them farter than unselect verts
 * 		if 0, unselected vertice are given the bias
 * strict: if 1, the vertice corresponding to the sel parameter are ignored and not just biased 
 */
BMVert *EDBM_findnearestvert(ViewContext *vc, int *dist, short sel, short strict)
{
	if(vc->v3d->drawtype>OB_WIRE && (vc->v3d->flag & V3D_ZBUF_SELECT)){
		int distance;
		unsigned int index;
		BMVert *eve;
		
		if(strict) index = view3d_sample_backbuf_rect(vc, vc->mval, 50, bm_wireoffs, 0xFFFFFF, &distance, strict, vc->em, findnearestvert__backbufIndextest); 
		else index = view3d_sample_backbuf_rect(vc, vc->mval, 50, bm_wireoffs, 0xFFFFFF, &distance, 0, NULL, NULL); 
		
		eve = BMIter_AtIndex(vc->em->bm, BM_VERTS_OF_MESH, NULL, index-1);
		
		if(eve && distance < *dist) {
			*dist = distance;
			return eve;
		} else {
			return NULL;
		}
			
	}
	else {
		struct { short mval[2], pass, select, strict; int dist, lastIndex, closestIndex; BMVert *closest; } data;
		static int lastSelectedIndex=0;
		static BMVert *lastSelected=NULL;
		
		if (lastSelected && BMIter_AtIndex(vc->em->bm, BM_VERTS_OF_MESH, NULL, lastSelectedIndex)!=lastSelected) {
			lastSelectedIndex = 0;
			lastSelected = NULL;
		}

		data.lastIndex = lastSelectedIndex;
		data.mval[0] = vc->mval[0];
		data.mval[1] = vc->mval[1];
		data.select = sel;
		data.dist = *dist;
		data.strict = strict;
		data.closest = NULL;
		data.closestIndex = 0;

		data.pass = 0;
		mesh_foreachScreenVert(vc, findnearestvert__doClosest, &data, 1);

		if (data.dist>3) {
			data.pass = 1;
			mesh_foreachScreenVert(vc, findnearestvert__doClosest, &data, 1);
		}

		*dist = data.dist;
		lastSelected = data.closest;
		lastSelectedIndex = data.closestIndex;

		return data.closest;
	}
}

/* returns labda for closest distance v1 to line-piece v2-v3 */
static float labda_PdistVL2Dfl( float *v1, float *v2, float *v3) 
{
	float rc[2], len;
	
	rc[0]= v3[0]-v2[0];
	rc[1]= v3[1]-v2[1];
	len= rc[0]*rc[0]+ rc[1]*rc[1];
	if(len==0.0f)
		return 0.0f;
	
	return ( rc[0]*(v1[0]-v2[0]) + rc[1]*(v1[1]-v2[1]) )/len;
}

/* note; uses v3d, so needs active 3d window */
static void findnearestedge__doClosest(void *userData, BMEdge *eed, int x0, int y0, int x1, int y1, int index)
{
	struct { ViewContext vc; float mval[2]; int dist; BMEdge *closest; } *data = userData;
	float v1[2], v2[2];
	int distance;
		
	v1[0] = x0;
	v1[1] = y0;
	v2[0] = x1;
	v2[1] = y1;
		
	distance= PdistVL2Dfl(data->mval, v1, v2);
		
	if(BM_TestHFlag(eed, BM_SELECT)) distance+=5;
	if(distance < data->dist) {
		if(data->vc.rv3d->rflag & RV3D_CLIPPING) {
			float labda= labda_PdistVL2Dfl(data->mval, v1, v2);
			float vec[3];

			vec[0]= eed->v1->co[0] + labda*(eed->v2->co[0] - eed->v1->co[0]);
			vec[1]= eed->v1->co[1] + labda*(eed->v2->co[1] - eed->v1->co[1]);
			vec[2]= eed->v1->co[2] + labda*(eed->v2->co[2] - eed->v1->co[2]);
			Mat4MulVecfl(data->vc.obedit->obmat, vec);

			if(view3d_test_clipping(data->vc.rv3d, vec)==0) {
				data->dist = distance;
				data->closest = eed;
			}
		}
		else {
			data->dist = distance;
			data->closest = eed;
		}
	}
}
BMEdge *EDBM_findnearestedge(ViewContext *vc, int *dist)
{

	if(vc->v3d->drawtype>OB_WIRE && (vc->v3d->flag & V3D_ZBUF_SELECT)) {
		int distance;
		unsigned int index = view3d_sample_backbuf_rect(vc, vc->mval, 50, bm_solidoffs, bm_wireoffs, &distance,0, NULL, NULL);
		BMEdge *eed = BMIter_AtIndex(vc->em->bm, BM_EDGES_OF_MESH, NULL, index-1);

		if (eed && distance<*dist) {
			*dist = distance;
			return eed;
		} else {
			return NULL;
		}
	}
	else {
		struct { ViewContext vc; float mval[2]; int dist; BMEdge *closest; } data;

		data.vc= *vc;
		data.mval[0] = vc->mval[0];
		data.mval[1] = vc->mval[1];
		data.dist = *dist;
		data.closest = NULL;

		mesh_foreachScreenEdge(vc, findnearestedge__doClosest, &data, 2);

		*dist = data.dist;
		return data.closest;
	}
}

static void findnearestface__getDistance(void *userData, BMFace *efa, int x, int y, int index)
{
	struct { short mval[2]; int dist; BMFace *toFace; } *data = userData;

	if (efa==data->toFace) {
		int temp = abs(data->mval[0]-x) + abs(data->mval[1]-y);

		if (temp<data->dist)
			data->dist = temp;
	}
}
static void findnearestface__doClosest(void *userData, BMFace *efa, int x, int y, int index)
{
	struct { short mval[2], pass; int dist, lastIndex, closestIndex; BMFace *closest; } *data = userData;

	if (data->pass==0) {
		if (index<=data->lastIndex)
			return;
	} else {
		if (index>data->lastIndex)
			return;
	}

	if (data->dist>3) {
		int temp = abs(data->mval[0]-x) + abs(data->mval[1]-y);

		if (temp<data->dist) {
			data->dist = temp;
			data->closest = efa;
			data->closestIndex = index;
		}
	}
}
static BMFace *EDBM_findnearestface(ViewContext *vc, int *dist)
{

	if(vc->v3d->drawtype>OB_WIRE && (vc->v3d->flag & V3D_ZBUF_SELECT)) {
		unsigned int index = view3d_sample_backbuf(vc, vc->mval[0], vc->mval[1]);
		BMFace *efa = BMIter_AtIndex(vc->em->bm, BM_FACES_OF_MESH, NULL, index-1);

		if (efa) {
			struct { short mval[2]; int dist; BMFace *toFace; } data;

			data.mval[0] = vc->mval[0];
			data.mval[1] = vc->mval[1];
			data.dist = 0x7FFF;		/* largest short */
			data.toFace = efa;

			mesh_foreachScreenFace(vc, findnearestface__getDistance, &data);

			if(vc->em->selectmode == SCE_SELECT_FACE || data.dist<*dist) {	/* only faces, no dist check */
				*dist= data.dist;
				return efa;
			}
		}
		
		return NULL;
	}
	else {
		struct { short mval[2], pass; int dist, lastIndex, closestIndex; BMFace *closest; } data;
		static int lastSelectedIndex=0;
		static BMFace *lastSelected=NULL;

		if (lastSelected && BMIter_AtIndex(vc->em->bm, BM_FACES_OF_MESH, NULL, lastSelectedIndex)!=lastSelected) {
			lastSelectedIndex = 0;
			lastSelected = NULL;
		}

		data.lastIndex = lastSelectedIndex;
		data.mval[0] = vc->mval[0];
		data.mval[1] = vc->mval[1];
		data.dist = *dist;
		data.closest = NULL;
		data.closestIndex = 0;

		data.pass = 0;
		mesh_foreachScreenFace(vc, findnearestface__doClosest, &data);

		if (data.dist>3) {
			data.pass = 1;
			mesh_foreachScreenFace(vc, findnearestface__doClosest, &data);
		}

		*dist = data.dist;
		lastSelected = data.closest;
		lastSelectedIndex = data.closestIndex;

		return data.closest;
	}
}

/* best distance based on screen coords. 
   use em->selectmode to define how to use 
   selected vertices and edges get disadvantage
   return 1 if found one
*/
static int unified_findnearest(ViewContext *vc, BMVert **eve, BMEdge **eed, BMFace **efa) 
{
	BMEditMesh *em= vc->em;
	int dist= 75;
	
	*eve= NULL;
	*eed= NULL;
	*efa= NULL;
	
	/* no afterqueue (yet), so we check it now, otherwise the em_xxxofs indices are bad */
	view3d_validate_backbuf(vc);
	
	if(em->selectmode & SCE_SELECT_VERTEX)
		*eve= EDBM_findnearestvert(vc, &dist, BM_SELECT, 0);
	if(em->selectmode & SCE_SELECT_FACE)
		*efa= EDBM_findnearestface(vc, &dist);

	dist-= 20;	/* since edges select lines, we give dots advantage of 20 pix */
	if(em->selectmode & SCE_SELECT_EDGE)
		*eed= EDBM_findnearestedge(vc, &dist);

	/* return only one of 3 pointers, for frontbuffer redraws */
	if(*eed) {
		*efa= NULL; *eve= NULL;
	}
	else if(*efa) {
		*eve= NULL;
	}
	
	return (*eve || *eed || *efa);
}

/* ****************  SIMILAR "group" SELECTS. FACE, EDGE AND VERTEX ************** */

/* selects new faces/edges/verts based on the existing selection */

/* FACES GROUP */

static EnumPropertyItem prop_simface_types[] = {
	{SIMFACE_MATERIAL, "MATERIAL", 0, "Material", ""},
	{SIMFACE_IMAGE, "IMAGE", 0, "Image", ""},
	{SIMFACE_AREA, "AREA", 0, "Area", ""},
	{SIMFACE_PERIMETER, "PERIMETER", 0, "Perimeter", ""},
	{SIMFACE_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMFACE_COPLANAR, "COPLANAR", 0, "Co-planar", ""},
	{0, NULL, 0, NULL, NULL}
};

static int similar_face_select_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh*)ob->data)->edit_btmesh;
	BMOperator bmop;

	/* get the type from RNA */
	int type = RNA_enum_get(op->ptr, "type");

	float thresh = scene->toolsettings->select_thresh;

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_InitOpf(em, &bmop, op, "similarfaces faces=%hf type=%d thresh=%f", BM_SELECT, type, thresh);

	/* execute the operator */
	BMO_Exec_Op(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_clear_flag_all(em, BM_SELECT);

	/* select the output */
	BMO_HeaderFlag_Buffer(em->bm, &bmop, "faceout", BM_SELECT, BM_ALL);

	/* finish the operator */
	if( !EDBM_FinishOp(em, &bmop, op, 1) )
		return OPERATOR_CANCELLED;

	/* dependencies graph and notification stuff */
	DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_GEOM_SELECT, ob);

	/* we succeeded */
	return OPERATOR_FINISHED;
}	

/* ***************************************************** */

/* EDGE GROUP */

static EnumPropertyItem prop_simedge_types[] = {
	{SIMEDGE_LENGTH, "LENGTH", 0, "Length", ""},
	{SIMEDGE_DIR, "DIR", 0, "Direction", ""},
	{SIMEDGE_FACE, "FACE", 0, "Amount of Faces Around an Edge", ""},
	{SIMEDGE_FACE_ANGLE, "FACE_ANGLE", 0, "Face Angles", ""},
	{SIMEDGE_CREASE, "CREASE", 0, "Crease", ""},
	{SIMEDGE_SEAM, "SEAM", 0, "Seam", ""},
	{SIMEDGE_SHARP, "SHARP", 0, "Sharpness", ""},
	{0, NULL, 0, NULL, NULL}
};

/* wrap the above function but do selection flushing edge to face */
static int similar_edge_select_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh*)ob->data)->edit_btmesh;
	BMOperator bmop;

	/* get the type from RNA */
	int type = RNA_enum_get(op->ptr, "type");

	float thresh = scene->toolsettings->select_thresh;

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_InitOpf(em, &bmop, op, "similaredges edges=%he type=%d thresh=%f", BM_SELECT, type, thresh);

	/* execute the operator */
	BMO_Exec_Op(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_clear_flag_all(em, BM_SELECT);

	/* select the output */
	BMO_HeaderFlag_Buffer(em->bm, &bmop, "edgeout", BM_SELECT, BM_ALL);
	EDBM_selectmode_flush(em);

	/* finish the operator */
	if( !EDBM_FinishOp(em, &bmop, op, 1) )
		return OPERATOR_CANCELLED;

	/* dependencies graph and notification stuff */
	DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_GEOM_SELECT, ob);

	/* we succeeded */
	return OPERATOR_FINISHED;
}

/* ********************************* */

/*
VERT GROUP
 mode 1: same normal
 mode 2: same number of face users
 mode 3: same vertex groups
*/

static EnumPropertyItem prop_simvertex_types[] = {
	{SIMVERT_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMVERT_FACE, "FACE", 0, "Amount of Adjacent Faces", ""},
	{SIMVERT_VGROUP, "VGROUP", 0, "Vertex Groups", ""},
	{0, NULL, 0, NULL, NULL}
};


static int similar_vert_select_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_edit_object(C);
	BMEditMesh *em = ((Mesh*)ob->data)->edit_btmesh;
	BMOperator bmop;
	/* get the type from RNA */
	int type = RNA_enum_get(op->ptr, "type");
	float thresh = scene->toolsettings->select_thresh;

	/* initialize the bmop using EDBM api, which does various ui error reporting and other stuff */
	EDBM_InitOpf(em, &bmop, op, "similarverts verts=%hv type=%d thresh=%f", BM_SELECT, type, thresh);

	/* execute the operator */
	BMO_Exec_Op(em->bm, &bmop);

	/* clear the existing selection */
	EDBM_clear_flag_all(em, BM_SELECT);

	/* select the output */
	BMO_HeaderFlag_Buffer(em->bm, &bmop, "vertout", BM_SELECT, BM_ALL);

	/* finish the operator */
	if( !EDBM_FinishOp(em, &bmop, op, 1) )
		return OPERATOR_CANCELLED;

	EDBM_selectmode_flush(em);

	/* dependencies graph and notification stuff */
	DAG_object_flush_update(scene, ob, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT | ND_GEOM_SELECT, ob);

	/* we succeeded */
	return OPERATOR_FINISHED;
}

static int select_similar_exec(bContext *C, wmOperator *op)
{
	int type= RNA_enum_get(op->ptr, "type");

	if(type < 100)
		return similar_vert_select_exec(C, op);
	else if(type < 200)
		return similar_edge_select_exec(C, op);
	else
		return similar_face_select_exec(C, op);
}

static EnumPropertyItem *select_similar_type_itemf(bContext *C, PointerRNA *ptr, int *free)
{
	Object *obedit;
	EnumPropertyItem *item= NULL;
	int totitem= 0;
	
	if(C==NULL) {
		/* needed for doc generation */
		RNA_enum_items_add(&item, &totitem, prop_simvertex_types);
		RNA_enum_items_add(&item, &totitem, prop_simedge_types);
		RNA_enum_items_add(&item, &totitem, prop_simface_types);
		RNA_enum_item_end(&item, &totitem);
		*free= 1;
		
		return item;
	}
	
	obedit= CTX_data_edit_object(C);
	
	if(obedit && obedit->type == OB_MESH) {
		BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh; 

		if(em->selectmode & SCE_SELECT_VERTEX)
			RNA_enum_items_add(&item, &totitem, prop_simvertex_types);
		else if(em->selectmode & SCE_SELECT_EDGE)
			RNA_enum_items_add(&item, &totitem, prop_simedge_types);
		else if(em->selectmode & SCE_SELECT_FACE)
			RNA_enum_items_add(&item, &totitem, prop_simface_types);
		RNA_enum_item_end(&item, &totitem);

		*free= 1;

		return item;
	}
	
	return NULL;
}

void MESH_OT_select_similar(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Select Similar";
	ot->idname= "MESH_OT_select_similar";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= select_similar_exec;
	ot->poll= ED_operator_editmesh;
	ot->description= "Select similar vertices, edges or faces by property types.";
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "type", prop_simvertex_types, 0, "Type", "");
	RNA_def_enum_funcs(prop, select_similar_type_itemf);
}

/* ***************************************************** */

/* ****************  LOOP SELECTS *************** */
/*faceloop_select, edgeloop_select, and edgering_select, are left
  here for reference purposes temporarily, but have all been replaced
  by uses of walker_select.*/

static void walker_select(BMEditMesh *em, int walkercode, void *start, int select)
{
	BMesh *bm = em->bm;
	BMHeader *h;
	BMWalker walker;

	BMW_Init(&walker, bm, walkercode, 0, 0);
	h = BMW_Begin(&walker, start);
	for (; h; h=BMW_Step(&walker)) {
		BM_Select(bm, h, select);
	}
	BMW_End(&walker);
}

#if 0
/* selects quads in loop direction of indicated edge */
/* only flush over edges with valence <= 2 */
void faceloop_select(EditMesh *em, EditEdge *startedge, int select)
{
	EditEdge *eed;
	EditFace *efa;
	int looking= 1;
	
	/* in eed->f1 we put the valence (amount of faces in edge) */
	/* in eed->f2 we put tagged flag as correct loop */
	/* in efa->f1 we put tagged flag as correct to select */

	for(eed= em->edges.first; eed; eed= eed->next) {
		eed->f1= 0;
		eed->f2= 0;
	}
	for(efa= em->faces.first; efa; efa= efa->next) {
		efa->f1= 0;
		if(efa->h==0) {
			efa->e1->f1++;
			efa->e2->f1++;
			efa->e3->f1++;
			if(efa->e4) efa->e4->f1++;
		}
	}
	
	/* tag startedge OK*/
	startedge->f2= 1;
	
	while(looking) {
		looking= 0;
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->h==0 && efa->e4 && efa->f1==0) {	/* not done quad */
				if(efa->e1->f1<=2 && efa->e2->f1<=2 && efa->e3->f1<=2 && efa->e4->f1<=2) { /* valence ok */

					/* if edge tagged, select opposing edge and mark face ok */
					if(efa->e1->f2) {
						efa->e3->f2= 1;
						efa->f1= 1;
						looking= 1;
					}
					else if(efa->e2->f2) {
						efa->e4->f2= 1;
						efa->f1= 1;
						looking= 1;
					}
					if(efa->e3->f2) {
						efa->e1->f2= 1;
						efa->f1= 1;
						looking= 1;
					}
					if(efa->e4->f2) {
						efa->e2->f2= 1;
						efa->f1= 1;
						looking= 1;
					}
				}
			}
		}
	}
	
	/* (de)select the faces */
	if(select!=2) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f1) EM_select_face(efa, select);
		}
	}
}
#endif


/* selects or deselects edges that:
- if edges has 2 faces:
	- has vertices with valence of 4
	- not shares face with previous edge
- if edge has 1 face:
	- has vertices with valence 4
	- not shares face with previous edge
	- but also only 1 face
- if edge no face:
	- has vertices with valence 2
*/

/* 
   Almostly exactly the same code as faceloop select
*/
static void edgering_select(BMEditMesh *em, BMEdge *startedge, int select)
{
#if 0 //BMESH_TODO
	BMEdge *eed;
	BMFace *efa;
	int looking= 1;
	
	/* in eed->f1 we put the valence (amount of faces in edge) */
	/* in eed->f2 we put tagged flag as correct loop */
	/* in efa->f1 we put tagged flag as correct to select */

	for(eed= em->edges.first; eed; eed= eed->next) {
		eed->f1= 0;
		eed->f2= 0;
	}
	for(efa= em->faces.first; efa; efa= efa->next) {
		efa->f1= 0;
		if(efa->h==0) {
			efa->e1->f1++;
			efa->e2->f1++;
			efa->e3->f1++;
			if(efa->e4) efa->e4->f1++;
		}
	}
	
	/* tag startedge OK */
	startedge->f2= 1;
	
	while(looking) {
		looking= 0;
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->e4 && efa->f1==0 && !efa->h) {	/* not done quad */
				if(efa->e1->f1<=2 && efa->e2->f1<=2 && efa->e3->f1<=2 && efa->e4->f1<=2) { /* valence ok */

					/* if edge tagged, select opposing edge and mark face ok */
					if(efa->e1->f2) {
						efa->e3->f2= 1;
						efa->f1= 1;
						looking= 1;
					}
					else if(efa->e2->f2) {
						efa->e4->f2= 1;
						efa->f1= 1;
						looking= 1;
					}
					if(efa->e3->f2) {
						efa->e1->f2= 1;
						efa->f1= 1;
						looking= 1;
					}
					if(efa->e4->f2) {
						efa->e2->f2= 1;
						efa->f1= 1;
						looking= 1;
					}
				}
			}
		}
	}
	
	/* (de)select the edges */
	for(eed= em->edges.first; eed; eed= eed->next) {
    		if(eed->f2) EM_select_edge(eed, select);
	}
#endif
}

static int loop_multiselect(bContext *C, wmOperator *op)
{
#if 0 //BMESH_TODO
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= EM_GetBMEditMesh(((Mesh *)obedit->data));
	BMEdge *eed;
	BMEdge **edarray;
	int edindex, edfirstcount;
	int looptype= RNA_boolean_get(op->ptr, "ring");
	
	/* sets em->totedgesel */
	EM_nedges_selected(em);
	
	edarray = MEM_mallocN(sizeof(BMEdge*)*em->totedgesel,"edge array");
	edindex = 0;
	edfirstcount = em->totedgesel;
	
	for(eed=em->edges.first; eed; eed=eed->next){
		if(eed->f&SELECT){
			edarray[edindex] = eed;
			edindex += 1;
		}
	}
	
	if(looptype){
		for(edindex = 0; edindex < edfirstcount; edindex +=1){
			eed = edarray[edindex];
			edgering_select(em, eed,SELECT);
		}
		EM_selectmode_flush(em);
	}
	else{
		for(edindex = 0; edindex < edfirstcount; edindex +=1){
			eed = edarray[edindex];
			edgeloop_select(em, eed,SELECT);
		}
		EM_selectmode_flush(em);
	}
	MEM_freeN(edarray);
//	if (EM_texFaceCheck())
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	EM_EndBMEditMesh(obedit->data, em);
#endif
	return OPERATOR_FINISHED;	
}

void MESH_OT_loop_multi_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Multi Select Loops";
	ot->idname= "MESH_OT_loop_multi_select";
	
	/* api callbacks */
	ot->exec= loop_multiselect;
	ot->poll= ED_operator_editmesh;
	ot->description= "Select a loop of connected edges by connection type.";
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "ring", 0, "Ring", "");
}

		
/* ***************** MAIN MOUSE SELECTION ************** */


/* ***************** loop select (non modal) ************** */

static void mouse_mesh_loop(bContext *C, short mval[2], short extend, short ring)
{
	ViewContext vc;
	BMEditMesh *em;
	BMEdge *eed;
	int select= 1;
	int dist= 50;
	
	em_setup_viewcontext(C, &vc);
	vc.mval[0]= mval[0];
	vc.mval[1]= mval[1];
	em= vc.em;
	
	eed= EDBM_findnearestedge(&vc, &dist);
	if(eed) {
		if(extend==0) EDBM_clear_flag_all(em, BM_SELECT);
	
		if(BM_TestHFlag(em, BM_SELECT)==0) select=1;
		else if(extend) select=0;

		if(em->selectmode & SCE_SELECT_FACE) {
			walker_select(em, BMW_FACELOOP, eed, select);
		}
		else if(em->selectmode & SCE_SELECT_EDGE) {
			if(ring)
				walker_select(em, BMW_EDGERING, eed, select);
			else
				walker_select(em, BMW_LOOP, eed, select);
		}
		else if(em->selectmode & SCE_SELECT_VERTEX) {
			if(ring)
				walker_select(em, BMW_EDGERING, eed, select);
			else 
				walker_select(em, BMW_LOOP, eed, select);
		}

		EDBM_selectmode_flush(em);
//			if (EM_texFaceCheck())
		
		WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, vc.obedit);
	}
}

static int mesh_select_loop_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	
	view3d_operator_needs_opengl(C);
	
	mouse_mesh_loop(C, event->mval, RNA_boolean_get(op->ptr, "extend"),
					RNA_boolean_get(op->ptr, "ring"));
	
	/* cannot do tweaks for as long this keymap is after transform map */
	return OPERATOR_FINISHED;
}

void MESH_OT_loop_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Loop Select";
	ot->idname= "MESH_OT_loop_select";
	
	/* api callbacks */
	ot->invoke= mesh_select_loop_invoke;
	ot->poll= ED_operator_editmesh;
	ot->description= "Select a loop of connected edges.";
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", "");
	RNA_def_boolean(ot->srna, "ring", 0, "Select Ring", "");
}

/* ******************* mesh shortest path select, uses prev-selected edge ****************** */

/* since you want to create paths with multiple selects, it doesn't have extend option */
static void mouse_mesh_shortest_path(bContext *C, short mval[2])
{
#if 0 //BMESH_TODO
	ViewContext vc;
	BMEditMesh *em;
	BMEdge *eed;
	int dist= 50;
	
	em_setup_viewcontext(C, &vc);
	vc.mval[0]= mval[0];
	vc.mval[1]= mval[1];
	em= vc.em;
	
	eed= findnearestedge(&vc, &dist);
	if(eed) {
		Mesh *me= vc.obedit->data;
		int path = 0;
		
		if (em->bm->selected.last) {
			EditSelection *ese = em->bm->selected.last;
			
			if(ese && ese->type == BMEdge) {
				BMEdge *eed_act;
				eed_act = (BMEdge*)ese->data;
				if (eed_act != eed) {
					if (edgetag_shortest_path(vc.scene, em, eed_act, eed)) {
						EM_remove_selection(em, eed_act, BMEdge);
						path = 1;
					}
				}
			}
		}
		if (path==0) {
			int act = (edgetag_context_check(vc.scene, eed)==0);
			edgetag_context_set(vc.scene, eed, act); /* switch the edge option */
		}
		
		EM_selectmode_flush(em);

		/* even if this is selected it may not be in the selection list */
		if(edgetag_context_check(vc.scene, eed)==0)
			EDBM_remove_selection(em, eed);
		else
			EDBM_store_selection(em, eed);
	
		/* force drawmode for mesh */
		switch (vc.scene->toolsettings->edge_mode) {
			
			case EDGE_MODE_TAG_SEAM:
				me->drawflag |= ME_DRAWSEAMS;
				break;
			case EDGE_MODE_TAG_SHARP:
				me->drawflag |= ME_DRAWSHARP;
				break;
			case EDGE_MODE_TAG_CREASE:	
				me->drawflag |= ME_DRAWCREASES;
				break;
			case EDGE_MODE_TAG_BEVEL:
				me->drawflag |= ME_DRAWBWEIGHTS;
				break;
		}
		
		DAG_object_flush_update(vc.scene, vc.obedit, OB_RECALC_DATA);
	
		WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, vc.obedit);
	}
#endif
}


static int mesh_shortest_path_select_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	
	view3d_operator_needs_opengl(C);

	mouse_mesh_shortest_path(C, event->mval);
	
	return OPERATOR_FINISHED;
}
	
void MESH_OT_select_shortest_path(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Shortest Path Select";
	ot->idname= "MESH_OT_select_shortest_path";
	
	/* api callbacks */
	ot->invoke= mesh_shortest_path_select_invoke;
	ot->poll= ED_operator_editmesh;
	ot->description= "Select shortest path between two selections.";
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", "");
}


/* ************************************************** */
/* here actual select happens */
/* gets called via generic mouse select operator */
void mouse_mesh(bContext *C, short mval[2], short extend)
{
	ViewContext vc;
	BMVert *eve = NULL;
	BMEdge *eed = NULL;
	BMFace *efa = NULL;
	
	/* setup view context for argument to callbacks */
	em_setup_viewcontext(C, &vc);
	vc.mval[0]= mval[0];
	vc.mval[1]= mval[1];
	
	if(unified_findnearest(&vc, &eve, &eed, &efa)) {
		
		if(extend==0) EDBM_clear_flag_all(vc.em, BM_SELECT);
		
		if(efa) {
			/* set the last selected face */
			EDBM_set_actFace(vc.em, efa);
			
			if(!BM_TestHFlag(efa, BM_SELECT)) {
				EDBM_store_selection(vc.em, efa);
				BM_Select(vc.em->bm, efa, 1);
			}
			else if(extend) {
				EDBM_remove_selection(vc.em, efa);
				BM_Select(vc.em->bm, efa, 0);
			}
		}
		else if(eed) {
			if(!BM_TestHFlag(eed, BM_SELECT)) {
				EDBM_store_selection(vc.em, eed);
				BM_Select(vc.em->bm, eed, 1);
			}
			else if(extend) {
				EDBM_remove_selection(vc.em, eed);
				BM_Select(vc.em->bm, eed, 0);
			}
		}
		else if(eve) {
			if(!BM_TestHFlag(eve, BM_SELECT)) {
				EDBM_store_selection(vc.em, eve);
				BM_Select(vc.em->bm, eve, 1);
			}
			else if(extend){ 
				EDBM_remove_selection(vc.em, eve);
				BM_Select(vc.em->bm, eve, 0);
			}
		}
		
		EDBM_selectmode_flush(vc.em);
		  
//		if (EM_texFaceCheck()) {

		if (efa && efa->mat_nr != vc.obedit->actcol-1) {
			vc.obedit->actcol= efa->mat_nr+1;
			vc.em->mat_nr= efa->mat_nr;
//			BIF_preview_changed(ID_MA);
		}
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, vc.obedit);
}

static void EDBM_strip_selections(BMEditMesh *em)
{
	BMEditSelection *ese, *nextese;

	if(!(em->selectmode & SCE_SELECT_VERTEX)){
		ese = em->bm->selected.first;
		while(ese){
			nextese = ese->next; 
			if(ese->type == BM_VERT) BLI_freelinkN(&(em->bm->selected),ese);
			ese = nextese;
		}
	}
	if(!(em->selectmode & SCE_SELECT_EDGE)){
		ese=em->bm->selected.first;
		while(ese){
			nextese = ese->next;
			if(ese->type == BM_EDGE) BLI_freelinkN(&(em->bm->selected), ese);
			ese = nextese;
		}
	}
	if(!(em->selectmode & SCE_SELECT_FACE)){
		ese=em->bm->selected.first;
		while(ese){
			nextese = ese->next;
			if(ese->type == BM_FACE) BLI_freelinkN(&(em->bm->selected), ese);
			ese = nextese;
		}
	}
}

/* when switching select mode, makes sure selection is consistant for editing */
/* also for paranoia checks to make sure edge or face mode works */
void EDBM_selectmode_set(BMEditMesh *em)
{
	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	BMIter iter;
	
	em->bm->selectmode = em->selectmode;

	EDBM_strip_selections(em); /*strip BMEditSelections from em->selected that are not relevant to new mode*/
	
	if(em->selectmode & SCE_SELECT_VERTEX) {
		/*BMIter iter;
		
		eed = BMIter_New(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
		for ( ; eed; eed=BMIter_Step(&iter)) BM_Select(em->bm, eed, 0);
		
		efa = BMIter_New(&iter, em->bm, BM_FACES_OF_MESH, NULL);
		for ( ; efa; efa=BMIter_Step(&iter)) BM_Select(em->bm, efa, 0);*/

		EDBM_selectmode_flush(em);
	}
	else if(em->selectmode & SCE_SELECT_EDGE) {
		/* deselect vertices, and select again based on edge select */
		eve = BMIter_New(&iter, em->bm, BM_VERTS_OF_MESH, NULL);
		for ( ; eve; eve=BMIter_Step(&iter)) BM_Select(em->bm, eve, 0);
		
		eed = BMIter_New(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
		for ( ; eed; eed=BMIter_Step(&iter)) {
			if (BM_TestHFlag(eed, BM_SELECT))
				BM_Select(em->bm, eed, 1);
		}
		
		/* selects faces based on edge status */
		EDBM_selectmode_flush(em);
	}
	else if(em->selectmode & SCE_SELECT_FACE) {
		/* deselect eges, and select again based on face select */
		eed = BMIter_New(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
		for ( ; eed; eed=BMIter_Step(&iter)) BM_Select(em->bm, eed, 0);
		
		efa = BMIter_New(&iter, em->bm, BM_FACES_OF_MESH, NULL);
		for ( ; efa; efa=BMIter_Step(&iter)) {
			if (BM_TestHFlag(efa, BM_SELECT))
				BM_Select(em->bm, efa, 1);
		}
	}
}

void EDBM_convertsel(BMEditMesh *em, short oldmode, short selectmode)
{
	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	BMIter iter;

	/*have to find out what the selectionmode was previously*/
	if(oldmode == SCE_SELECT_VERTEX) {
		if(selectmode == SCE_SELECT_EDGE) {
			/*select all edges associated with every selected vertex*/
			eed = BMIter_New(&iter, em->bm, BM_EDGES_OF_MESH, NULL);
			for ( ; eed; eed=BMIter_Step(&iter)) {
				if(BM_TestHFlag(eed->v1, BM_SELECT)) BM_Select(em->bm, eed, 1);
				else if(BM_TestHFlag(eed->v2, BM_SELECT)) BM_Select(em->bm, eed, 1);
			}
		}		
		else if(selectmode == SCE_SELECT_FACE) {
			BMIter liter;
			BMLoop *l;

			/*select all faces associated with every selected vertex*/
			efa = BMIter_New(&iter, em->bm, BM_FACES_OF_MESH, NULL);
			for ( ; efa; efa=BMIter_Step(&iter)) {
				l = BMIter_New(&liter, em->bm, BM_LOOPS_OF_FACE, efa);
				for (; l; l=BMIter_Step(&liter)) {
					if (BM_TestHFlag(l->v, BM_SELECT)) {
						BM_Select(em->bm, efa, 1);
						break;
					}
				}
			}
		}
	}
	
	if(oldmode == SCE_SELECT_EDGE){
		if(selectmode == SCE_SELECT_FACE) {
			BMIter liter;
			BMLoop *l;

			/*select all faces associated with every selected vertex*/
			efa = BMIter_New(&iter, em->bm, BM_FACES_OF_MESH, NULL);
			for ( ; efa; efa=BMIter_Step(&iter)) {
				l = BMIter_New(&liter, em->bm, BM_LOOPS_OF_FACE, efa);
				for (; l; l=BMIter_Step(&liter)) {
					if (BM_TestHFlag(l->v, BM_SELECT)) {
						BM_Select(em->bm, efa, 1);
						break;
					}
				}
			}
		}
	}
}


void EDBM_select_swap(BMEditMesh *em) /* exported for UV */
{
	BMIter iter;
	BMVert *eve;
	BMEdge *eed;
	BMFace *efa;
	
	if(em->bm->selectmode & SCE_SELECT_VERTEX) {
		BM_ITER(eve, &iter, em->bm, BM_VERTS_OF_MESH, NULL) {
			if (BM_TestHFlag(eve, BM_HIDDEN))
				continue;
			BM_Select(em->bm, eve, !BM_TestHFlag(eve, BM_SELECT));
		}
	}
	else if(em->selectmode & SCE_SELECT_EDGE) {
		BM_ITER(eed, &iter, em->bm, BM_EDGES_OF_MESH, NULL) {
			if (BM_TestHFlag(eed, BM_HIDDEN))
				continue;
			BM_Select(em->bm, eed, !BM_TestHFlag(eed, BM_SELECT));
		}
	}
	else {
		BM_ITER(efa, &iter, em->bm, BM_FACES_OF_MESH, NULL) {
			if (BM_TestHFlag(efa, BM_HIDDEN))
				continue;
			BM_Select(em->bm, efa, !BM_TestHFlag(efa, BM_SELECT));
		}

	}
//	if (EM_texFaceCheck())
}

static int select_inverse_mesh_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh *)obedit->data)->edit_btmesh;
	
	EDBM_select_swap(em);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	return OPERATOR_FINISHED;	
}

void MESH_OT_select_inverse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Inverse";
	ot->idname= "MESH_OT_select_inverse";
	ot->description= "Select inverse of (un)selected vertices, edges or faces.";
	
	/* api callbacks */
	ot->exec= select_inverse_mesh_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


static int select_linked_pick_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *obedit= CTX_data_edit_object(C);
	ViewContext vc;
	BMWalker walker;
	BMEditMesh *em;
	BMVert *eve;
	BMEdge *e, *eed;
	BMFace *efa;
	short done=1, toggle=0;
	int sel= !RNA_boolean_get(op->ptr, "deselect");
	int limit= RNA_boolean_get(op->ptr, "limit");
	
	/* unified_finednearest needs ogl */
	view3d_operator_needs_opengl(C);
	
	/* setup view context for argument to callbacks */
	em_setup_viewcontext(C, &vc);
	em = vc.em;

	if(vc.em->bm->totedge==0)
		return OPERATOR_CANCELLED;
	
	vc.mval[0]= event->mval[0];
	vc.mval[1]= event->mval[1];
	
	/* return warning! */

	/*if(limit) {
		int retval= select_linked_limited_invoke(&vc, 0, sel);
		WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
		return retval;
	}*/
	
	if( unified_findnearest(&vc, &eve, &eed, &efa)==0 ) {
		WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	
		return OPERATOR_CANCELLED;
	}
	
	if (efa) {
		eed = efa->loopbase->e;
	} else if (!eed) {
		if (!eve || !eve->edge)
			return OPERATOR_CANCELLED;
		
		eed = eve->edge;
	}

	BMW_Init(&walker, em->bm, BMW_SHELL, 0, 0);
	e = BMW_Begin(&walker, eed->v1);
	for (; e; e=BMW_Step(&walker)) {
			BM_Select(em->bm, e->v1, sel);
			BM_Select(em->bm, e->v2, sel);
	}
	BMW_End(&walker);
	EDBM_select_flush(em, SCE_SELECT_VERTEX);

	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	return OPERATOR_FINISHED;	
}

void MESH_OT_select_linked_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Linked";
	ot->idname= "MESH_OT_select_linked_pick";
	
	/* api callbacks */
	ot->invoke= select_linked_pick_invoke;
	ot->poll= ED_operator_editmesh;
	ot->description= "select/deselect all vertices linked to the edge under the mouse cursor.";
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "");
	RNA_def_boolean(ot->srna, "limit", 0, "Limit by Seams", "");
}


static int select_linked_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= ((Mesh*)obedit->data)->edit_btmesh;
	V_DECLARE(verts);
	BMVert **verts = NULL;
	BMIter iter;
	BMVert *v;
	BMEdge *e;
	BMWalker walker;
	int i, tot;

	tot = 0;
		BM_ITER_SELECT(v, &iter, em->bm, BM_VERTS_OF_MESH, NULL)
		if (BM_TestHFlag(v, BM_SELECT)) {
			V_GROW(verts);
			verts[tot++] = v;
		}
	}

	BMW_Init(&walker, em->bm, BMW_SHELL, 0, 0);
	for (i=0; i<tot; i++) {
		e = BMW_Begin(&walker, verts[i]);
		for (; e; e=BMW_Step(&walker)) {
			BM_Select(em->bm, e->v1, 1);
			BM_Select(em->bm, e->v2, 1);
		}
	}
	BMW_End(&walker);
	EDBM_select_flush(em, SCE_SELECT_VERTEX);

	V_FREE(verts);
	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);

	return OPERATOR_FINISHED;	
}

void MESH_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Linked All";
	ot->idname= "MESH_OT_select_linked";
	
	/* api callbacks */
	ot->exec= select_linked_exec;
	ot->poll= ED_operator_editmesh;
	ot->description= "Select all vertices linked to the active mesh.";
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "limit", 0, "Limit by Seams", "");
}

/* ******************** **************** */

static int select_more(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= (((Mesh *)obedit->data))->edit_btmesh;
	BMOperator bmop;
	int usefaces = em->selectmode > SCE_SELECT_EDGE;

	EDBM_InitOpf(em, &bmop, op, "regionextend geom=%hvef constrict=%d usefaces=%d", 
	             BM_SELECT, 0, usefaces);

	BMO_Exec_Op(em->bm, &bmop);
	BMO_HeaderFlag_Buffer(em->bm, &bmop, "geomout", BM_SELECT, BM_ALL);

	EDBM_selectmode_flush(em);

	if (!EDBM_FinishOp(em, &bmop, op, 1))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select More";
	ot->idname= "MESH_OT_select_more";
	ot->description= "Select more vertices, edges or faces connected to initial selection.";

	/* api callbacks */
	ot->exec= select_more;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int select_less(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	BMEditMesh *em= (((Mesh *)obedit->data))->edit_btmesh;
	BMOperator bmop;
	int usefaces = em->selectmode > SCE_SELECT_EDGE;

	EDBM_InitOpf(em, &bmop, op, "regionextend geom=%hvef constrict=%d usefaces=%d", 
	             BM_SELECT, 1, usefaces);

	BMO_Exec_Op(em->bm, &bmop);
	BMO_UnHeaderFlag_Buffer(em->bm, &bmop, "geomout", BM_SELECT, BM_ALL);

	EDBM_selectmode_flush(em);

	if (!EDBM_FinishOp(em, &bmop, op, 1))
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT|ND_GEOM_SELECT, obedit);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Less";
	ot->idname= "MESH_OT_select_less";
	ot->description= "Deselect vertices, edges or faces at the boundary of each selection region.";

	/* api callbacks */
	ot->exec= select_less;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
