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

editmesh_mods.c, UI level access, no geometry changes 

*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"



#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_context.h"
#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_paint.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_report.h"

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

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "mesh_intern.h"

#include "BLO_sys_types.h" // for intptr_t support

/* XXX */
static void waitcursor(int val) {}
static int pupmenu(const char *dummy) {return 0;}

/* ****************************** MIRROR **************** */

void EM_cache_x_mirror_vert(struct Object *ob, struct EditMesh *em)
{
	EditVert *eve, *eve_mirror;
	int index= 0;

	for(eve= em->verts.first; eve; eve= eve->next) {
		eve->tmp.v= NULL;
	}

	for(eve= em->verts.first; eve; eve= eve->next, index++) {
		if(eve->tmp.v==NULL) {
			eve_mirror = editmesh_get_x_mirror_vert(ob, em, eve, eve->co, index);
			if(eve_mirror) {
				eve->tmp.v= eve_mirror;
				eve_mirror->tmp.v = eve;
			}
		}
	}
}

static void EM_select_mirrored(Object *obedit, EditMesh *em, int extend)
{

	EditVert *eve;

	EM_cache_x_mirror_vert(obedit, em);

	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->f & SELECT && eve->tmp.v && (eve->tmp.v != eve->tmp.v->tmp.v)) {
			eve->tmp.v->f |= SELECT;

			if(extend==FALSE)
				eve->f &= ~SELECT;

			/* remove the interference */
			eve->tmp.v->tmp.v= NULL;
			eve->tmp.v= NULL;
		}
	}
}

void EM_automerge(Scene *scene, Object *obedit, int update)
{
	Mesh *me= obedit ? obedit->data : NULL; /* can be NULL */
	int len;

	if ((scene->toolsettings->automerge) &&
		(obedit && obedit->type==OB_MESH && (obedit->mode & OB_MODE_EDIT)) &&
		(me->mr==NULL)
	  ) {
		Mesh *me= (Mesh*)obedit->data;
		EditMesh *em= me->edit_mesh;

		len = removedoublesflag(em, 1, 1, scene->toolsettings->doublimit);
		if (len) {
			em->totvert -= len; /* saves doing a countall */
			if (update) {
				DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
			}
		}
	}
}

/* ****************************** SELECTION ROUTINES **************** */

unsigned int em_solidoffs=0, em_wireoffs=0, em_vertoffs=0;	/* set in drawobject.c ... for colorindices */

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
int EM_init_backbuf_border(ViewContext *vc, short xmin, short ymin, short xmax, short ymax)
{
	struct ImBuf *buf;
	unsigned int *dr;
	int a;
	
	if(vc->obedit==NULL || vc->v3d->drawtype<OB_SOLID || (vc->v3d->flag & V3D_ZBUF_SELECT)==0) return 0;
	
	buf= view3d_read_backbuf(vc, xmin, ymin, xmax, ymax);
	if(buf==NULL) return 0;
	if(em_vertoffs==0) return 0;

	dr = buf->rect;
	
	/* build selection lookup */
	selbuf= MEM_callocN(em_vertoffs+1, "selbuf");
	
	a= (xmax-xmin+1)*(ymax-ymin+1);
	while(a--) {
		if(*dr>0 && *dr<=em_vertoffs) 
			selbuf[*dr]= 1;
		dr++;
	}
	IMB_freeImBuf(buf);
	return 1;
}

int EM_check_backbuf(unsigned int index)
{
	if(selbuf==NULL) return 1;
	if(index>0 && index<=em_vertoffs)
		return selbuf[index];
	return 0;
}

void EM_free_backbuf(void)
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
int EM_mask_init_backbuf_border(ViewContext *vc, short mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax)
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
	if(em_vertoffs==0) return 0;

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
	selbuf= MEM_callocN(em_vertoffs+1, "selbuf");
	
	a= (xmax-xmin+1)*(ymax-ymin+1);
	while(a--) {
		if(*dr>0 && *dr<=em_vertoffs && *drm==0) selbuf[*dr]= 1;
		dr++; drm++;
	}
	IMB_freeImBuf(buf);
	IMB_freeImBuf(bufmask);
	return 1;
	
}

/* circle shaped sample area */
int EM_init_backbuf_circle(ViewContext *vc, short xs, short ys, short rads)
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
	if(em_vertoffs==0) return 0;
	if(buf==NULL) return 0;

	dr = buf->rect;
	
	/* build selection lookup */
	selbuf= MEM_callocN(em_vertoffs+1, "selbuf");
	radsq= rads*rads;
	for(yc= -rads; yc<=rads; yc++) {
		for(xc= -rads; xc<=rads; xc++, dr++) {
			if(xc*xc + yc*yc < radsq) {
				if(*dr>0 && *dr<=em_vertoffs) selbuf[*dr]= 1;
			}
		}
	}

	IMB_freeImBuf(buf);
	return 1;
	
}

static void findnearestvert__doClosest(void *userData, EditVert *eve, int x, int y, int index)
{
	struct { short mval[2], pass, select, strict; int dist, lastIndex, closestIndex; EditVert *closest; } *data = userData;

	if (data->pass==0) {
		if (index<=data->lastIndex)
			return;
	} else {
		if (index>data->lastIndex)
			return;
	}

	if (data->dist>3) {
		int temp = abs(data->mval[0] - x) + abs(data->mval[1]- y);
		if ((eve->f&1) == data->select) {
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
	EditMesh *em= (EditMesh *)handle;
	EditVert *eve = BLI_findlink(&em->verts, index-1);

	if(eve && (eve->f & SELECT)) return 0;
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
EditVert *findnearestvert(ViewContext *vc, int *dist, short sel, short strict)
{
	if(vc->v3d->drawtype>OB_WIRE && (vc->v3d->flag & V3D_ZBUF_SELECT)){
		int distance;
		unsigned int index;
		EditVert *eve;
		
		if(strict) index = view3d_sample_backbuf_rect(vc, vc->mval, 50, em_wireoffs, 0xFFFFFF, &distance, strict, vc->em, findnearestvert__backbufIndextest); 
		else index = view3d_sample_backbuf_rect(vc, vc->mval, 50, em_wireoffs, 0xFFFFFF, &distance, 0, NULL, NULL); 
		
		eve = BLI_findlink(&vc->em->verts, index-1);
		
		if(eve && distance < *dist) {
			*dist = distance;
			return eve;
		} else {
			return NULL;
		}
			
	}
	else {
		struct { short mval[2], pass, select, strict; int dist, lastIndex, closestIndex; EditVert *closest; } data;
		static int lastSelectedIndex=0;
		static EditVert *lastSelected=NULL;

		if (lastSelected && BLI_findlink(&vc->em->verts, lastSelectedIndex)!=lastSelected) {
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

		ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);

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
static void findnearestedge__doClosest(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index)
{
	struct { ViewContext vc; float mval[2]; int dist; EditEdge *closest; } *data = userData;
	float v1[2], v2[2];
	int distance;

	ED_view3d_local_clipping(data->vc.rv3d, data->vc.obedit->obmat); /* for local clipping lookups */

	v1[0] = x0;
	v1[1] = y0;
	v2[0] = x1;
	v2[1] = y1;

	distance= dist_to_line_segment_v2(data->mval, v1, v2);


	if(eed->f & SELECT) distance+=5;
	if(distance < data->dist) {
		if(data->vc.rv3d->rflag & RV3D_CLIPPING) {
			float labda= labda_PdistVL2Dfl(data->mval, v1, v2);
			float vec[3];

			vec[0]= eed->v1->co[0] + labda*(eed->v2->co[0] - eed->v1->co[0]);
			vec[1]= eed->v1->co[1] + labda*(eed->v2->co[1] - eed->v1->co[1]);
			vec[2]= eed->v1->co[2] + labda*(eed->v2->co[2] - eed->v1->co[2]);

			if(view3d_test_clipping(data->vc.rv3d, vec, 1)==0) {
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
EditEdge *findnearestedge(ViewContext *vc, int *dist)
{

	if(vc->v3d->drawtype>OB_WIRE && (vc->v3d->flag & V3D_ZBUF_SELECT)) {
		int distance;
		unsigned int index = view3d_sample_backbuf_rect(vc, vc->mval, 50, em_solidoffs, em_wireoffs, &distance,0, NULL, NULL);
		EditEdge *eed = BLI_findlink(&vc->em->edges, index-1);

		if (eed && distance<*dist) {
			*dist = distance;
			return eed;
		} else {
			return NULL;
		}
	}
	else {
		struct { ViewContext vc; float mval[2]; int dist; EditEdge *closest; } data;

		data.vc= *vc;
		data.mval[0] = vc->mval[0];
		data.mval[1] = vc->mval[1];
		data.dist = *dist;
		data.closest = NULL;

		ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
		mesh_foreachScreenEdge(vc, findnearestedge__doClosest, &data, 2);

		*dist = data.dist;
		return data.closest;
	}
}

static void findnearestface__getDistance(void *userData, EditFace *efa, int x, int y, int index)
{
	struct { short mval[2]; int dist; EditFace *toFace; } *data = userData;

	if (efa==data->toFace) {
		int temp = abs(data->mval[0]-x) + abs(data->mval[1]-y);

		if (temp<data->dist)
			data->dist = temp;
	}
}
static void findnearestface__doClosest(void *userData, EditFace *efa, int x, int y, int index)
{
	struct { short mval[2], pass; int dist, lastIndex, closestIndex; EditFace *closest; } *data = userData;

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
static EditFace *findnearestface(ViewContext *vc, int *dist)
{

	if(vc->v3d->drawtype>OB_WIRE && (vc->v3d->flag & V3D_ZBUF_SELECT)) {
		unsigned int index = view3d_sample_backbuf(vc, vc->mval[0], vc->mval[1]);
		EditFace *efa = BLI_findlink(&vc->em->faces, index-1);

		if (efa) {
			struct { short mval[2]; int dist; EditFace *toFace; } data;

			data.mval[0] = vc->mval[0];
			data.mval[1] = vc->mval[1];
			data.dist = 0x7FFF;		/* largest short */
			data.toFace = efa;

			ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
			mesh_foreachScreenFace(vc, findnearestface__getDistance, &data);

			if(vc->em->selectmode == SCE_SELECT_FACE || data.dist<*dist) {	/* only faces, no dist check */
				*dist= data.dist;
				return efa;
			}
		}
		
		return NULL;
	}
	else {
		struct { short mval[2], pass; int dist, lastIndex, closestIndex; EditFace *closest; } data;
		static int lastSelectedIndex=0;
		static EditFace *lastSelected=NULL;

		if (lastSelected && BLI_findlink(&vc->em->faces, lastSelectedIndex)!=lastSelected) {
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

		ED_view3d_init_mats_rv3d(vc->obedit, vc->rv3d);
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
static int unified_findnearest(ViewContext *vc, EditVert **eve, EditEdge **eed, EditFace **efa) 
{
	EditMesh *em= vc->em;
	int dist= 75;
	
	*eve= NULL;
	*eed= NULL;
	*efa= NULL;
	
	/* no afterqueue (yet), so we check it now, otherwise the em_xxxofs indices are bad */
	view3d_validate_backbuf(vc);
	
	if(em->selectmode & SCE_SELECT_VERTEX)
		*eve= findnearestvert(vc, &dist, SELECT, 0);
	if(em->selectmode & SCE_SELECT_FACE)
		*efa= findnearestface(vc, &dist);

	dist-= 20;	/* since edges select lines, we give dots advantage of 20 pix */
	if(em->selectmode & SCE_SELECT_EDGE)
		*eed= findnearestedge(vc, &dist);

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

/* VERT GROUP */

#define SIMVERT_NORMAL	0
#define SIMVERT_FACE	1
#define SIMVERT_VGROUP	2
#define SIMVERT_TOT		3

/* EDGE GROUP */

#define SIMEDGE_LENGTH		101
#define SIMEDGE_DIR			102
#define SIMEDGE_FACE		103
#define SIMEDGE_FACE_ANGLE	104
#define SIMEDGE_CREASE		105
#define SIMEDGE_SEAM		106
#define SIMEDGE_SHARP		107
#define SIMEDGE_TOT			108

/* FACE GROUP */

#define SIMFACE_MATERIAL	201
#define SIMFACE_IMAGE		202
#define SIMFACE_AREA		203
#define SIMFACE_PERIMETER	204
#define SIMFACE_NORMAL		205
#define SIMFACE_COPLANAR	206
#define SIMFACE_TOT			207

static EnumPropertyItem prop_similar_types[] = {
	{SIMVERT_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMVERT_FACE, "FACE", 0, "Amount of Vertices in Face", ""},
	{SIMVERT_VGROUP, "VGROUP", 0, "Vertex Groups", ""},
	{SIMEDGE_LENGTH, "LENGTH", 0, "Length", ""},
	{SIMEDGE_DIR, "DIR", 0, "Direction", ""},
	{SIMEDGE_FACE, "FACE", 0, "Amount of Vertices in Face", ""},
	{SIMEDGE_FACE_ANGLE, "FACE_ANGLE", 0, "Face Angles", ""},
	{SIMEDGE_CREASE, "CREASE", 0, "Crease", ""},
	{SIMEDGE_SEAM, "SEAM", 0, "Seam", ""},
	{SIMEDGE_SHARP, "SHARP", 0, "Sharpness", ""},
	{SIMFACE_MATERIAL, "MATERIAL", 0, "Material", ""},
	{SIMFACE_IMAGE, "IMAGE", 0, "Image", ""},
	{SIMFACE_AREA, "AREA", 0, "Area", ""},
	{SIMFACE_PERIMETER, "PERIMETER", 0, "Perimeter", ""},
	{SIMFACE_NORMAL, "NORMAL", 0, "Normal", ""},
	{SIMFACE_COPLANAR, "COPLANAR", 0, "Co-planar", ""},
	{0, NULL, 0, NULL, NULL}
};


/* this as a way to compare the ares, perim  of 2 faces thay will scale to different sizes
*0.5 so smaller faces arnt ALWAYS selected with a thresh of 1.0 */
#define SCALE_CMP(a,b) ((a+a*thresh >= b) && (a-(a*thresh*0.5) <= b))

static int similar_face_select__internal(Scene *scene, EditMesh *em, int mode)
{
	EditFace *efa, *base_efa=NULL;
	unsigned int selcount=0; /*count how many new faces we select*/
	
	/*deselcount, count how many deselected faces are left, so we can bail out early
	also means that if there are no deselected faces, we can avoid a lot of looping */
	unsigned int deselcount=0; 
	float thresh= scene->toolsettings->select_thresh;
	short ok=0;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		if (!efa->h) {
			if (efa->f & SELECT) {
				efa->f1=1;
				ok=1;
			} else {
				efa->f1=0;
				deselcount++; /* a deselected face we may select later */
			}
		}
	}
	
	if (!ok || !deselcount) /* no data selected OR no more data to select */
		return 0;
	
	if (mode==SIMFACE_AREA) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			efa->tmp.fp= EM_face_area(efa);
		}
	} else if (mode==SIMFACE_PERIMETER) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			efa->tmp.fp= EM_face_perimeter(efa);
		}
	}
	
	for(base_efa= em->faces.first; base_efa; base_efa= base_efa->next) {
		if (base_efa->f1) { /* This was one of the faces originaly selected */
			if (mode==SIMFACE_MATERIAL) { /* same material */
				for(efa= em->faces.first; efa; efa= efa->next) {
					if (
						!(efa->f & SELECT) &&
						!efa->h &&
						base_efa->mat_nr == efa->mat_nr
					) {
						EM_select_face(efa, 1);
						selcount++;
						deselcount--;
						if (!deselcount) /*have we selected all posible faces?, if so return*/
							return selcount;
					}
				}
			} else if (mode==SIMFACE_IMAGE) { /* same image */
				MTFace *tf, *base_tf;

				base_tf = (MTFace*)CustomData_em_get(&em->fdata, base_efa->data,
													 CD_MTFACE);

				if(!base_tf)
					return selcount;

				for(efa= em->faces.first; efa; efa= efa->next) {
					if (!(efa->f & SELECT) && !efa->h) {
						tf = (MTFace*)CustomData_em_get(&em->fdata, efa->data,
														CD_MTFACE);

						if(base_tf->tpage == tf->tpage) {
							EM_select_face(efa, 1);
							selcount++;
							deselcount--;
							if (!deselcount) /*have we selected all posible faces?, if so return*/
								return selcount;
						}
					}
				}
			} else if (mode==SIMFACE_AREA || mode==SIMFACE_PERIMETER) { /* same area OR same perimeter, both use the same temp var */
				for(efa= em->faces.first; efa; efa= efa->next) {
					if (
						(!(efa->f & SELECT) && !efa->h) &&
						SCALE_CMP(base_efa->tmp.fp, efa->tmp.fp)
					) {
						EM_select_face(efa, 1);
						selcount++;
						deselcount--;
						if (!deselcount) /*have we selected all posible faces?, if so return*/
							return selcount;
					}
				}
			} else if (mode==SIMFACE_NORMAL) {
				float angle;
				for(efa= em->faces.first; efa; efa= efa->next) {
					if (!(efa->f & SELECT) && !efa->h) {
						angle= RAD2DEG(angle_v2v2(base_efa->n, efa->n));
						if (angle/180.0<=thresh) {
							EM_select_face(efa, 1);
							selcount++;
							deselcount--;
							if (!deselcount) /*have we selected all posible faces?, if so return*/
								return selcount;
						}
					}
				}
			} else if (mode==SIMFACE_COPLANAR) { /* same planer */
				float angle, base_dot, dot;
				base_dot= dot_v3v3(base_efa->cent, base_efa->n);
				for(efa= em->faces.first; efa; efa= efa->next) {
					if (!(efa->f & SELECT) && !efa->h) {
						angle= RAD2DEG(angle_v2v2(base_efa->n, efa->n));
						if (angle/180.0<=thresh) {
							dot=dot_v3v3(efa->cent, base_efa->n);
							if (fabs(base_dot-dot) <= thresh) {
								EM_select_face(efa, 1);
								selcount++;
								deselcount--;
								if (!deselcount) /*have we selected all posible faces?, if so return*/
									return selcount;
							}
						}
					}
				}
			}
		}
	} /* end base_efa loop */
	return selcount;
}

static int similar_face_select_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Mesh *me= obedit->data;
	EditMesh *em= BKE_mesh_get_editmesh(me); 

	int selcount = similar_face_select__internal(scene, em, RNA_int_get(op->ptr, "type"));
	
	if (selcount) {
		/* here was an edge-mode only select flush case, has to be generalized */
		EM_selectmode_flush(em);
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
		BKE_mesh_end_editmesh(me, em);
		return OPERATOR_FINISHED;
	}
	
	BKE_mesh_end_editmesh(me, em);
	return OPERATOR_CANCELLED;
}	

/* ***************************************************** */

static int similar_edge_select__internal(ToolSettings *ts, EditMesh *em, int mode)
{
	EditEdge *eed, *base_eed=NULL;
	unsigned int selcount=0; /* count how many new edges we select*/
	
	/*count how many visible selected edges there are,
	so we can return when there are none left */
	unsigned int deselcount=0;
	
	short ok=0;
	float thresh= ts->select_thresh;
	
	for(eed= em->edges.first; eed; eed= eed->next) {
		if (!eed->h) {
			if (eed->f & SELECT) {
				eed->f1=1;
				ok=1;
			} else {
				eed->f1=0;
				deselcount++;
			}
			/* set all eed->tmp.l to 0 we use it later.
			for counting face users*/
			eed->tmp.l=0;
			eed->f2=0; /* only for mode SIMEDGE_FACE_ANGLE, edge animations */
		}
	}
	
	if (!ok || !deselcount) /* no data selected OR no more data to select*/
		return 0;
	
	if (mode==SIMEDGE_LENGTH) { /*store length*/
		for(eed= em->edges.first; eed; eed= eed->next) {
			if (!eed->h) /* dont calc data for hidden edges*/
				eed->tmp.fp= len_v3v3(eed->v1->co, eed->v2->co);
		}
	} else if (mode==SIMEDGE_FACE) { /*store face users*/
		EditFace *efa;
		/* cound how many faces each edge uses use tmp->l */
		for(efa= em->faces.first; efa; efa= efa->next) {
			efa->e1->tmp.l++;
			efa->e2->tmp.l++;
			efa->e3->tmp.l++;
			if (efa->e4) efa->e4->tmp.l++;
		}
	} else if (mode==SIMEDGE_FACE_ANGLE) { /*store edge angles */
		EditFace *efa;
		int j;
		/* cound how many faces each edge uses use tmp.l */
		for(efa= em->faces.first; efa; efa= efa->next) {
			/* here we use the edges temp data to assign a face
			if a face has alredy been assigned (eed->f2==1)
			we calculate the angle between the current face and
			the edges previously found face.
			store the angle in eed->tmp.fp (loosing the face eed->tmp.f)
			but tagging eed->f2==2, so we know not to look at it again.
			This only works for edges that connect to 2 faces. but its good enough
			*/
			
			/* se we can loop through face edges*/
			j=0;
			eed= efa->e1;
			while (j<4) {
				if (j==1) eed= efa->e2;
				else if (j==2) eed= efa->e3;
				else if (j==3) {
					eed= efa->e4;
					if (!eed)
						break;
				} /* done looping */
				
				if (!eed->h) { /* dont calc data for hidden edges*/
					if (eed->f2==2)
						break;
					else if (eed->f2==0) /* first access, assign the face */
						eed->tmp.f= efa;
					else if (eed->f2==1) /* second, we assign the angle*/
						eed->tmp.fp= RAD2DEG(angle_v2v2(eed->tmp.f->n, efa->n))/180;
					eed->f2++; /* f2==0 no face assigned. f2==1 one face found. f2==2 angle calculated.*/
				}
				j++;
			}
		}
	}
	
	for(base_eed= em->edges.first; base_eed; base_eed= base_eed->next) {
		if (base_eed->f1) {
			if (mode==SIMEDGE_LENGTH) { /* same length */
				for(eed= em->edges.first; eed; eed= eed->next) {
					if (
						!(eed->f & SELECT) &&
						!eed->h &&
						SCALE_CMP(base_eed->tmp.fp, eed->tmp.fp)
					) {
						EM_select_edge(eed, 1);
						selcount++;
						deselcount--;
						if (!deselcount) /*have we selected all posible faces?, if so return*/
							return selcount;
					}
				}
			} else if (mode==SIMEDGE_DIR) { /* same direction */
				float base_dir[3], dir[3], angle;
				sub_v3_v3v3(base_dir, base_eed->v1->co, base_eed->v2->co);
				for(eed= em->edges.first; eed; eed= eed->next) {
					if (!(eed->f & SELECT) && !eed->h) {
						sub_v3_v3v3(dir, eed->v1->co, eed->v2->co);
						angle= RAD2DEG(angle_v2v2(base_dir, dir));
						
						if (angle>90) /* use the smallest angle between the edges */
							angle= fabs(angle-180.0f);
						
						if (angle/90.0<=thresh) {
							EM_select_edge(eed, 1);
							selcount++;
							deselcount--;
							if (!deselcount) /*have we selected all posible faces?, if so return*/
								return selcount;
						}
					}
				}
			} else if (mode==SIMEDGE_FACE) { /* face users */				
				for(eed= em->edges.first; eed; eed= eed->next) {
					if (
						!(eed->f & SELECT) &&
						!eed->h &&
						base_eed->tmp.l==eed->tmp.l
					) {
						EM_select_edge(eed, 1);
						selcount++;
						deselcount--;
						if (!deselcount) /*have we selected all posible faces?, if so return*/
							return selcount;
					}
				}
			} else if (mode==SIMEDGE_FACE_ANGLE && base_eed->f2==2) { /* edge angles, f2==2 means the edge has an angle. */				
				for(eed= em->edges.first; eed; eed= eed->next) {
					if (
						!(eed->f & SELECT) &&
						!eed->h &&
						eed->f2==2 &&
						(fabs(base_eed->tmp.fp-eed->tmp.fp)<=thresh)
					) {
						EM_select_edge(eed, 1);
						selcount++;
						deselcount--;
						if (!deselcount) /*have we selected all posible faces?, if so return*/
							return selcount;
					}
				}
			} else if (mode==SIMEDGE_CREASE) { /* edge crease */
				for(eed= em->edges.first; eed; eed= eed->next) {
					if (
						!(eed->f & SELECT) &&
						!eed->h &&
						(fabs(base_eed->crease-eed->crease) <= thresh)
					) {
						EM_select_edge(eed, 1);
						selcount++;
						deselcount--;
						if (!deselcount) /*have we selected all posible faces?, if so return*/
							return selcount;
					}
				}
			} else if (mode==SIMEDGE_SEAM) { /* edge seam */
				for(eed= em->edges.first; eed; eed= eed->next) {
					if (
						!(eed->f & SELECT) &&
						!eed->h &&
						(eed->seam == base_eed->seam)
					) {
						EM_select_edge(eed, 1);
						selcount++;
						deselcount--;
						if (!deselcount) /*have we selected all posible faces?, if so return*/
							return selcount;
					}
				}
			} else if (mode==SIMEDGE_SHARP) { /* edge sharp */
				for(eed= em->edges.first; eed; eed= eed->next) {
					if (
						!(eed->f & SELECT) &&
						!eed->h &&
						(eed->sharp == base_eed->sharp)
					) {
						EM_select_edge(eed, 1);
						selcount++;
						deselcount--;
						if (!deselcount) /*have we selected all posible faces?, if so return*/
							return selcount;
					}
				}
			}
		}
	}	
	return selcount;
}
/* wrap the above function but do selection flushing edge to face */
static int similar_edge_select_exec(bContext *C, wmOperator *op)
{
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	Mesh *me= obedit->data;
	EditMesh *em= BKE_mesh_get_editmesh(me); 

	int selcount = similar_edge_select__internal(ts, em, RNA_int_get(op->ptr, "type"));
	
	if (selcount) {
		/* here was an edge-mode only select flush case, has to be generalized */
		EM_selectmode_flush(em);
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
		BKE_mesh_end_editmesh(me, em);
		return OPERATOR_FINISHED;
	}
	
	BKE_mesh_end_editmesh(me, em);
	return OPERATOR_CANCELLED;
}

/* ********************************* */

static int similar_vert_select_exec(bContext *C, wmOperator *op)
{
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *obedit= CTX_data_edit_object(C);
	Mesh *me= obedit->data;
	EditMesh *em= BKE_mesh_get_editmesh(me); 
	EditVert *eve, *base_eve=NULL;
	unsigned int selcount=0; /* count how many new edges we select*/
	
	/*count how many visible selected edges there are,
	so we can return when there are none left */
	unsigned int deselcount=0;
	int mode= RNA_enum_get(op->ptr, "type");
	
	short ok=0;
	float thresh= ts->select_thresh;
	
	for(eve= em->verts.first; eve; eve= eve->next) {
		if (!eve->h) {
			if (eve->f & SELECT) {
				eve->f1=1;
				ok=1;
			} else {
				eve->f1=0;
				deselcount++;
			}
			/* set all eve->tmp.l to 0 we use them later.*/
			eve->tmp.l=0;
		}
		
	}
	
	if (!ok || !deselcount) { /* no data selected OR no more data to select*/
		BKE_mesh_end_editmesh(me, em);
		return 0;
	}
	
	if(mode == SIMVERT_FACE) {
		/* store face users */
		EditFace *efa;
		
		/* count how many faces each edge uses use tmp->l */
		for(efa= em->faces.first; efa; efa= efa->next) {
			efa->v1->tmp.l++;
			efa->v2->tmp.l++;
			efa->v3->tmp.l++;
			if (efa->v4) efa->v4->tmp.l++;
		}
	}
	
	
	for(base_eve= em->verts.first; base_eve; base_eve= base_eve->next) {
		if (base_eve->f1) {
				
			if(mode == SIMVERT_NORMAL) {
				float angle;
				for(eve= em->verts.first; eve; eve= eve->next) {
					if (!(eve->f & SELECT) && !eve->h) {
						angle= RAD2DEG(angle_v2v2(base_eve->no, eve->no));
						if (angle/180.0<=thresh) {
							eve->f |= SELECT;
							selcount++;
							deselcount--;
							if (!deselcount) {/*have we selected all posible faces?, if so return*/
								BKE_mesh_end_editmesh(me, em);
								return selcount;
							}
						}
					}
				}
			}
			else if(mode == SIMVERT_FACE) {
				for(eve= em->verts.first; eve; eve= eve->next) {
					if (
						!(eve->f & SELECT) &&
						!eve->h &&
						base_eve->tmp.l==eve->tmp.l
					) {
						eve->f |= SELECT;
						selcount++;
						deselcount--;
						if (!deselcount) {/*have we selected all posible faces?, if so return*/
							BKE_mesh_end_editmesh(me, em);
							return selcount;
						}
					}
				}
			} 
			else if(mode == SIMVERT_VGROUP) {
				MDeformVert *dvert, *base_dvert;
				short i, j; /* weight index */

				base_dvert= CustomData_em_get(&em->vdata, base_eve->data,
					CD_MDEFORMVERT);

				if (!base_dvert || base_dvert->totweight == 0) {
					BKE_mesh_end_editmesh(me, em);
					return selcount;
				}
				
				for(eve= em->verts.first; eve; eve= eve->next) {
					dvert= CustomData_em_get(&em->vdata, eve->data,
						CD_MDEFORMVERT);

					if (dvert && !(eve->f & SELECT) && !eve->h && dvert->totweight) {
						/* do the extra check for selection in the following if, so were not
						checking verts that may be alredy selected */
						for (i=0; base_dvert->totweight >i && !(eve->f & SELECT); i++) { 
							for (j=0; dvert->totweight >j; j++) {
								if (base_dvert->dw[i].def_nr==dvert->dw[j].def_nr) {
									eve->f |= SELECT;
									selcount++;
									deselcount--;
									if (!deselcount) { /*have we selected all posible faces?, if so return*/
										BKE_mesh_end_editmesh(me, em);
										return selcount;
									}
									break;
								}
							}
						}
					}
				}
			}
		}
	} /* end basevert loop */

	if(selcount) {
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
		BKE_mesh_end_editmesh(me, em);
		return OPERATOR_FINISHED;
	}

	BKE_mesh_end_editmesh(me, em);
	return OPERATOR_CANCELLED;
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
	Object *obedit= CTX_data_edit_object(C);
	EnumPropertyItem *item= NULL;
	int a, totitem= 0;

	if (C == NULL) {
		return prop_similar_types;
	}
		
	if(obedit && obedit->type == OB_MESH) {
		EditMesh *em= BKE_mesh_get_editmesh(obedit->data); 

		if(em->selectmode & SCE_SELECT_VERTEX) {
			for(a=SIMVERT_NORMAL; a<=SIMVERT_TOT; a++)
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
		}
		else if(em->selectmode & SCE_SELECT_EDGE) {
			for(a=SIMEDGE_LENGTH; a<=SIMEDGE_TOT; a++)
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
		}
		else if(em->selectmode & SCE_SELECT_FACE) {
			for(a=SIMFACE_MATERIAL; a<=SIMFACE_TOT; a++)
				RNA_enum_items_add_value(&item, &totitem, prop_similar_types, a);
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*free= 1;

	return item;
}

void MESH_OT_select_similar(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Select Similar";
	ot->description= "Select similar vertices, edges or faces by property types";
	ot->idname= "MESH_OT_select_similar";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= select_similar_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "type", prop_similar_types, SIMVERT_NORMAL, "Type", "");
	RNA_def_enum_funcs(prop, select_similar_type_itemf);
	ot->prop= prop;
}

/* ******************************************* */


int mesh_layers_menu_charlen(CustomData *data, int type)
{
	 int i, len = 0;
	/* see if there is a duplicate */
	for(i=0; i<data->totlayer; i++) {
		if((&data->layers[i])->type == type) {
			/* we could count the chars here but we'll just assumeme each
			 * is 32 chars with some room for the menu text - 40 should be fine */
			len+=40; 
		}
	}
	return len;
}

/* this function adds menu text into an existing string.
 * this string's size should be allocated with mesh_layers_menu_charlen */
void mesh_layers_menu_concat(CustomData *data, int type, char *str) 
{
	int i, count = 0;
	char *str_pt = str;
	CustomDataLayer *layer;
	
	/* see if there is a duplicate */
	for(i=0; i<data->totlayer; i++) {
		layer = &data->layers[i];
		if(layer->type == type) {
			str_pt += sprintf(str_pt, "%s%%x%d|", layer->name, count);
			count++;
		}
	}
}

int mesh_layers_menu(CustomData *data, int type) {
	int ret;
	char *str_pt, *str;
	
	str_pt = str = MEM_mallocN(mesh_layers_menu_charlen(data, type) + 18, "layer menu");
	str[0] = '\0';
	
	str_pt += sprintf(str_pt, "Layers%%t|");
	
	mesh_layers_menu_concat(data, type, str_pt);
	
	ret = pupmenu(str);
	MEM_freeN(str);
	return ret;
}

void EM_mesh_copy_edge(EditMesh *em, short type) 
{
	EditSelection *ese;
	short change=0;
	
	EditEdge *eed, *eed_act;
	float vec[3], vec_mid[3], eed_len, eed_len_act;
	
	if (!em) return;
	
	ese = em->selected.last;
	if (!ese) return;
	
	eed_act = (EditEdge*)ese->data;
	
	switch (type) {
	case 1: /* copy crease */
		for(eed=em->edges.first; eed; eed=eed->next) {
			if (eed->f & SELECT && eed != eed_act && eed->crease != eed_act->crease) {
				eed->crease = eed_act->crease;
				change = 1;
			}
		}
		break;
	case 2: /* copy bevel weight */
		for(eed=em->edges.first; eed; eed=eed->next) {
			if (eed->f & SELECT && eed != eed_act && eed->bweight != eed_act->bweight) {
				eed->bweight = eed_act->bweight;
				change = 1;
			}
		}
		break;

	case 3: /* copy length */
		eed_len_act = len_v3v3(eed_act->v1->co, eed_act->v2->co);
		for(eed=em->edges.first; eed; eed=eed->next) {
			if (eed->f & SELECT && eed != eed_act) {

				eed_len = len_v3v3(eed->v1->co, eed->v2->co);

				if (eed_len == eed_len_act) continue;
				/* if this edge is zero length we cont do anything with it*/
				if (eed_len == 0.0f) continue;
				if (eed_len_act == 0.0f) {
					add_v3_v3v3(vec_mid, eed->v1->co, eed->v2->co);
					mul_v3_fl(vec_mid, 0.5);
					VECCOPY(eed->v1->co, vec_mid);
					VECCOPY(eed->v2->co, vec_mid);
				} else {
					/* copy the edge length */
					add_v3_v3v3(vec_mid, eed->v1->co, eed->v2->co);
					mul_v3_fl(vec_mid, 0.5);

					/* SCALE 1 */
					sub_v3_v3v3(vec, eed->v1->co, vec_mid);
					mul_v3_fl(vec, eed_len_act/eed_len);
					add_v3_v3v3(eed->v1->co, vec, vec_mid);

					/* SCALE 2 */
					sub_v3_v3v3(vec, eed->v2->co, vec_mid);
					mul_v3_fl(vec, eed_len_act/eed_len);
					add_v3_v3v3(eed->v2->co, vec, vec_mid);
				}
				change = 1;
			}
		}

		if (change)
			recalc_editnormals(em);

		break;
	}
	
	if (change) {
//		DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
		
	}
}

void EM_mesh_copy_face(EditMesh *em, wmOperator *op, short type)
{
	short change=0;
	
	EditFace *efa, *efa_act;
	MTFace *tf, *tf_act = NULL;
	MCol *mcol, *mcol_act = NULL;
	if (!em) return;
	efa_act = EM_get_actFace(em, 0);
	
	if (!efa_act) return;
	
	tf_act =	CustomData_em_get(&em->fdata, efa_act->data, CD_MTFACE);
	mcol_act =	CustomData_em_get(&em->fdata, efa_act->data, CD_MCOL);
	
	switch (type) {
	case 1: /* copy material */
		for(efa=em->faces.first; efa; efa=efa->next) {
			if (efa->f & SELECT && efa->mat_nr != efa_act->mat_nr) {
				efa->mat_nr = efa_act->mat_nr;
				change = 1;
			}
		}
		break;
	case 2:	/* copy image */
		if (!tf_act) {
			BKE_report(op->reports, RPT_ERROR, "Mesh has no uv/image layers.");
			return;
		}
		for(efa=em->faces.first; efa; efa=efa->next) {
			if (efa->f & SELECT && efa != efa_act) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if (tf_act->tpage) {
					tf->tpage = tf_act->tpage;
					tf->mode |= TF_TEX;
				} else {
					tf->tpage = NULL;
					tf->mode &= ~TF_TEX;
				}
				tf->tile= tf_act->tile;
				change = 1;
			}
		}
		break;

	case 3: /* copy UV's */
		if (!tf_act) {
			BKE_report(op->reports, RPT_ERROR, "Mesh has no uv/image layers.");
			return;
		}
		for(efa=em->faces.first; efa; efa=efa->next) {
			if (efa->f & SELECT && efa != efa_act) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				memcpy(tf->uv, tf_act->uv, sizeof(tf->uv));
				change = 1;
			}
		}
		break;
	case 4: /* mode's */
		if (!tf_act) {
			BKE_report(op->reports, RPT_ERROR, "Mesh has no uv/image layers.");
			return;
		}
		for(efa=em->faces.first; efa; efa=efa->next) {
			if (efa->f & SELECT && efa != efa_act) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				tf->mode= tf_act->mode;
				change = 1;
			}
		}
		break;
	case 5: /* copy transp's */
		if (!tf_act) {
			BKE_report(op->reports, RPT_ERROR, "Mesh has no uv/image layers.");
			return;
		}
		for(efa=em->faces.first; efa; efa=efa->next) {
			if (efa->f & SELECT && efa != efa_act) {
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				tf->transp= tf_act->transp;
				change = 1;
			}
		}
		break;

	case 6: /* copy vcols's */
		if (!mcol_act) {
			BKE_report(op->reports, RPT_ERROR, "Mesh has no color layers.");
			return;
		} else {
			/* guess the 4th color if needs be */
			float val =- 1;

			if (!efa_act->v4) {
				/* guess the othe vale, we may need to use it
				 * 
				 * Modifying the 4th value of the mcol is ok here since its not seen
				 * on a triangle
				 * */
				val = ((float)(mcol_act->r +  (mcol_act+1)->r + (mcol_act+2)->r)) / 3; CLAMP(val, 0, 255);
				(mcol_act+3)->r = (char)val;

				val = ((float)(mcol_act->g +  (mcol_act+1)->g + (mcol_act+2)->g)) / 3; CLAMP(val, 0, 255);
				(mcol_act+3)->g = (char)val;

				val = ((float)(mcol_act->b +  (mcol_act+1)->b + (mcol_act+2)->b)) / 3; CLAMP(val, 0, 255);
				(mcol_act+3)->b = (char)val;
			} 


			for(efa=em->faces.first; efa; efa=efa->next) {
				if (efa->f & SELECT && efa != efa_act) {
					/* TODO - make copy from tri to quad guess the 4th vert */
					mcol = CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
					memcpy(mcol, mcol_act, sizeof(MCol)*4);	
					change = 1;
				}
			}
		}
		break;
	}
	
	if (change) {
//		DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
		
	}
}


void EM_mesh_copy_face_layer(EditMesh *em, wmOperator *op, short type) 
{
	short change=0;
	
	EditFace *efa;
	MTFace *tf, *tf_from;
	MCol *mcol, *mcol_from;
	
	if (!em) return;
	
	switch(type) {
	case 7:
	case 8:
	case 9:
		if (CustomData_number_of_layers(&em->fdata, CD_MTFACE)<2) {
			BKE_report(op->reports, RPT_ERROR, "mesh does not have multiple uv/image layers");
			return;
		} else {
			int layer_orig_idx, layer_idx;

			layer_idx = mesh_layers_menu(&em->fdata, CD_MTFACE);
			if (layer_idx<0) return;

			/* warning, have not updated mesh pointers however this is not needed since we swicth back */
			layer_orig_idx = CustomData_get_active_layer(&em->fdata, CD_MTFACE);
			if (layer_idx==layer_orig_idx)
				return;

			/* get the tfaces */
			CustomData_set_layer_active(&em->fdata, CD_MTFACE, (int)layer_idx);
			/* store the tfaces in our temp */
			for(efa=em->faces.first; efa; efa=efa->next) {
				if (efa->f & SELECT) {
					efa->tmp.p = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				}	
			}
			CustomData_set_layer_active(&em->fdata, CD_MTFACE, layer_orig_idx);
		}
		break;

	case 10: /* select vcol layers - make sure this stays in sync with above code */
		if (CustomData_number_of_layers(&em->fdata, CD_MCOL)<2) {
			BKE_report(op->reports, RPT_ERROR, "mesh does not have multiple color layers");
			return;
		} else {
			int layer_orig_idx, layer_idx;

			layer_idx = mesh_layers_menu(&em->fdata, CD_MCOL);
			if (layer_idx<0) return;

			/* warning, have not updated mesh pointers however this is not needed since we swicth back */
			layer_orig_idx = CustomData_get_active_layer(&em->fdata, CD_MCOL);
			if (layer_idx==layer_orig_idx)
				return;

			/* get the tfaces */
			CustomData_set_layer_active(&em->fdata, CD_MCOL, (int)layer_idx);
			/* store the tfaces in our temp */
			for(efa=em->faces.first; efa; efa=efa->next) {
				if (efa->f & SELECT) {
					efa->tmp.p = CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
				}	
			}
			CustomData_set_layer_active(&em->fdata, CD_MCOL, layer_orig_idx);

		}
		break;
	}

	/* layer copy only - sanity checks done above */
	switch (type) {
	case 7: /* copy UV's only */
		for(efa=em->faces.first; efa; efa=efa->next) {
			if (efa->f & SELECT) {
				tf_from = (MTFace *)efa->tmp.p; /* not active but easier to use this way */
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				memcpy(tf->uv, tf_from->uv, sizeof(tf->uv));
				change = 1;
			}
		}
		break;
	case 8: /* copy image settings only */
		for(efa=em->faces.first; efa; efa=efa->next) {
			if (efa->f & SELECT) {
				tf_from = (MTFace *)efa->tmp.p; /* not active but easier to use this way */
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if (tf_from->tpage) {
					tf->tpage = tf_from->tpage;
					tf->mode |= TF_TEX;
				} else {
					tf->tpage = NULL;
					tf->mode &= ~TF_TEX;
				}
				tf->tile= tf_from->tile;
				change = 1;
			}
		}
		break;
	case 9: /* copy all tface info */
		for(efa=em->faces.first; efa; efa=efa->next) {
			if (efa->f & SELECT) {
				tf_from = (MTFace *)efa->tmp.p; /* not active but easier to use this way */
				tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				memcpy(tf->uv, ((MTFace *)efa->tmp.p)->uv, sizeof(tf->uv));
				tf->tpage = tf_from->tpage;
				tf->mode = tf_from->mode;
				tf->transp = tf_from->transp;
				change = 1;
			}
		}
		break;
	case 10:
		for(efa=em->faces.first; efa; efa=efa->next) {
			if (efa->f & SELECT) {
				mcol_from = (MCol *)efa->tmp.p; 
				mcol = CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
				memcpy(mcol, mcol_from, sizeof(MCol)*4);	
				change = 1;
			}
		}
		break;
	}

	if (change) {
//		DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
		
	}
}


/* ctrl+c in mesh editmode */
void mesh_copy_menu(EditMesh *em, wmOperator *op)
{
	EditSelection *ese;
	int ret;
	if (!em) return;
	
	ese = em->selected.last;
	
	/* Faces can have a NULL ese, so dont return on a NULL ese here */
	
	if(ese && ese->type == EDITVERT) {
		/* EditVert *ev, *ev_act = (EditVert*)ese->data;
		ret= pupmenu(""); */
	} else if(ese && ese->type == EDITEDGE) {
		ret= pupmenu("Copy Active Edge to Selected%t|Crease%x1|Bevel Weight%x2|Length%x3");
		if (ret<1) return;
		
		EM_mesh_copy_edge(em, ret);
		
	} else if(ese==NULL || ese->type == EDITFACE) {
		ret= pupmenu(
			"Copy Face Selected%t|"
			"Active Material%x1|Active Image%x2|Active UV Coords%x3|"
			"Active Mode%x4|Active Transp%x5|Active Vertex Colors%x6|%l|"

			"TexFace UVs from layer%x7|"
			"TexFace Images from layer%x8|"
			"TexFace All from layer%x9|"
			"Vertex Colors from layer%x10");
		if (ret<1) return;
		
		if (ret<=6) {
			EM_mesh_copy_face(em, op, ret);
		} else {
			EM_mesh_copy_face_layer(em, op, ret);
		}
	}
}


/* ****************  LOOP SELECTS *************** */

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


/* helper for edgeloop_select, checks for eed->f2 tag in faces */
static int edge_not_in_tagged_face(EditMesh *em, EditEdge *eed)
{
	EditFace *efa;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->h==0) {
			if(efa->e1==eed || efa->e2==eed || efa->e3==eed || efa->e4==eed) {	/* edge is in face */
				if(efa->e1->f2 || efa->e2->f2 || efa->e3->f2 || (efa->e4 && efa->e4->f2)) {	/* face is tagged */
					return 0;
				}
			}
		}
	}
	return 1;
}

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
static void edgeloop_select(EditMesh *em, EditEdge *starteed, int select)
{
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	int looking= 1;
	
	/* in f1 we put the valence (amount of edges in a vertex, or faces in edge) */
	/* in eed->f2 and efa->f1 we put tagged flag as correct loop */
	for(eve= em->verts.first; eve; eve= eve->next) {
		eve->f1= 0;
		eve->f2= 0;
	}
	for(eed= em->edges.first; eed; eed= eed->next) {
		eed->f1= 0;
		eed->f2= 0;
		if((eed->h & 1)==0) {	/* fgon edges add to valence too */
			eed->v1->f1++; eed->v2->f1++;
		}
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
	
	/* looped edges & vertices get tagged f2 */
	starteed->f2= 1;
	if(starteed->v1->f1<5) starteed->v1->f2= 1;
	if(starteed->v2->f1<5) starteed->v2->f2= 1;
	/* sorry, first edge isnt even ok */
	if(starteed->v1->f2==0 && starteed->v2->f2==0) looking= 0;
	
	while(looking) {
		looking= 0;
		
		/* find correct valence edges which are not tagged yet, but connect to tagged one */
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->h==0 && eed->f2==0) { /* edge not hidden, not tagged */
				if( (eed->v1->f1<5 && eed->v1->f2) || (eed->v2->f1<5 && eed->v2->f2)) { /* valence of vertex OK, and is tagged */
					/* new edge is not allowed to be in face with tagged edge */
					if(edge_not_in_tagged_face(em, eed)) {
						if(eed->f1==starteed->f1) {	/* same amount of faces */
							looking= 1;
							eed->f2= 1;
							if(eed->v2->f1<5) eed->v2->f2= 1;
							if(eed->v1->f1<5) eed->v1->f2= 1;
						}
					}
				}
			}
		}
	}
	/* and we do the select */
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->f2) EM_select_edge(eed, select);
	}
}

/* 
   Almostly exactly the same code as faceloop select
*/
static void edgering_select(EditMesh *em, EditEdge *startedge, int select)
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
}

static int loop_multiselect(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	EditEdge *eed;
	EditEdge **edarray;
	int edindex, edfirstcount;
	int looptype= RNA_boolean_get(op->ptr, "ring");
	
	/* sets em->totedgesel */
	EM_nedges_selected(em);
	
	edarray = MEM_mallocN(sizeof(EditEdge*)*em->totedgesel,"edge array");
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
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;	
}

void MESH_OT_loop_multi_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Multi Select Loops";
	ot->description= "Select a loop of connected edges by connection type";
	ot->idname= "MESH_OT_loop_multi_select";
	
	/* api callbacks */
	ot->exec= loop_multiselect;
	ot->poll= ED_operator_editmesh;
	
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
	EditMesh *em;
	EditEdge *eed;
	int select= 1;
	int dist= 50;
	
	em_setup_viewcontext(C, &vc);
	vc.mval[0]= mval[0];
	vc.mval[1]= mval[1];
	em= vc.em;

	/* no afterqueue (yet), so we check it now, otherwise the em_xxxofs indices are bad */
	view3d_validate_backbuf(&vc);
	
	eed= findnearestedge(&vc, &dist);
	if(eed) {
		if(extend==0) EM_clear_flag_all(em, SELECT);
	
		if((eed->f & SELECT)==0) select=1;
		else if(extend) select=0;

		if(em->selectmode & SCE_SELECT_FACE) {
			faceloop_select(em, eed, select);
		}
		else if(em->selectmode & SCE_SELECT_EDGE) {
			if(ring)
				edgering_select(em, eed, select);
			else
				edgeloop_select(em, eed, select);
		}
		else if(em->selectmode & SCE_SELECT_VERTEX) {
			if(ring)
				edgering_select(em, eed, select);
			else 
				edgeloop_select(em, eed, select);
		}

		EM_selectmode_flush(em);
//			if (EM_texFaceCheck())
		
		/* sets as active, useful for other tools */
		if(select && em->selectmode & SCE_SELECT_EDGE) {
			EM_store_selection(em, eed, EDITEDGE);
		}

		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, vc.obedit->data);
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
	ot->description= "Select a loop of connected edges";
	ot->idname= "MESH_OT_loop_select";
	
	/* api callbacks */
	ot->invoke= mesh_select_loop_invoke;
	ot->poll= ED_operator_editmesh_view3d;
	
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
	ViewContext vc;
	EditMesh *em;
	EditEdge *eed, *eed_act= NULL;
	int dist= 50;
	
	em_setup_viewcontext(C, &vc);
	vc.mval[0]= mval[0];
	vc.mval[1]= mval[1];
	em= vc.em;
	
	/* no afterqueue (yet), so we check it now, otherwise the em_xxxofs indices are bad */
	view3d_validate_backbuf(&vc);
	
	eed= findnearestedge(&vc, &dist);
	if(eed) {
		Mesh *me= vc.obedit->data;
		int path = 0;
		
		if (em->selected.last) {
			EditSelection *ese = em->selected.last;
			
			if(ese && ese->type == EDITEDGE) {
				eed_act = (EditEdge*)ese->data;
				if (eed_act != eed) {
					if (edgetag_shortest_path(vc.scene, em, eed_act, eed)) {
						EM_remove_selection(em, eed_act, EDITEDGE);
						path = 1;
					}
				}
			}
		}
		if (path==0) {
			int act = (edgetag_context_check(vc.scene, eed)==0);
			edgetag_context_set(vc.scene, eed, act); /* switch the edge option */
		}

		/* even if this is selected it may not be in the selection list */
		if(edgetag_context_check(vc.scene, eed)==EDGE_MODE_SELECT)
			EM_remove_selection(em, eed, EDITEDGE);
		else {
			/* other modes need to keep the last edge tagged */
			if(eed_act)
				EM_select_edge(eed_act, 0);

			EM_select_edge(eed, 1);
			EM_store_selection(em, eed, EDITEDGE);
		}

		EM_selectmode_flush(em);
	
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
		
		DAG_id_flush_update(vc.obedit->data, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, vc.obedit->data);
	}
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
	ot->description= "Select shortest path between two selections";
	ot->idname= "MESH_OT_select_shortest_path";
	
	/* api callbacks */
	ot->invoke= mesh_shortest_path_select_invoke;
	ot->poll= ED_operator_editmesh_view3d;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", "");
}


/* ************************************************** */


/* here actual select happens */
/* gets called via generic mouse select operator */
int mouse_mesh(bContext *C, short mval[2], short extend)
{
	ViewContext vc;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	
	/* setup view context for argument to callbacks */
	em_setup_viewcontext(C, &vc);
	vc.mval[0]= mval[0];
	vc.mval[1]= mval[1];
	
	if(unified_findnearest(&vc, &eve, &eed, &efa)) {
		
		if(extend==0) EM_clear_flag_all(vc.em, SELECT);
		
		if(efa) {
			/* set the last selected face */
			EM_set_actFace(vc.em, efa);
			
			if( (efa->f & SELECT)==0 ) {
				EM_store_selection(vc.em, efa, EDITFACE);
				EM_select_face_fgon(vc.em, efa, 1);
			}
			else if(extend) {
				EM_remove_selection(vc.em, efa, EDITFACE);
				EM_select_face_fgon(vc.em, efa, 0);
			}
		}
		else if(eed) {
			if((eed->f & SELECT)==0) {
				EM_store_selection(vc.em, eed, EDITEDGE);
				EM_select_edge(eed, 1);
			}
			else if(extend) {
				EM_remove_selection(vc.em, eed, EDITEDGE);
				EM_select_edge(eed, 0);
			}
		}
		else if(eve) {
			if((eve->f & SELECT)==0) {
				eve->f |= SELECT;
				EM_store_selection(vc.em, eve, EDITVERT);
			}
			else if(extend){ 
				EM_remove_selection(vc.em, eve, EDITVERT);
				eve->f &= ~SELECT;
			}
		}
		
		EM_selectmode_flush(vc.em);
		  
//		if (EM_texFaceCheck()) {

		if (efa && efa->mat_nr != vc.obedit->actcol-1) {
			vc.obedit->actcol= efa->mat_nr+1;
			vc.em->mat_nr= efa->mat_nr;
//			BIF_preview_changed(ID_MA);
		}

		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, vc.obedit->data);

		return 1;
	}
	
	return 0;
}

/* *********** select linked ************* */

/* for use with selectconnected_delimit_mesh only! */
#define is_edge_delimit_ok(eed) ((eed->tmp.l == 1) && (eed->seam==0))
#define is_face_tag(efa) is_edge_delimit_ok(efa->e1) || is_edge_delimit_ok(efa->e2) || is_edge_delimit_ok(efa->e3) || (efa->v4 && is_edge_delimit_ok(efa->e4))

#define face_tag(efa)\
if(efa->v4)	efa->tmp.l=		efa->e1->tmp.l= efa->e2->tmp.l= efa->e3->tmp.l= efa->e4->tmp.l= 1;\
else		efa->tmp.l=		efa->e1->tmp.l= efa->e2->tmp.l= efa->e3->tmp.l= 1;

/* all - 1) use all faces for extending the selection  2) only use the mouse face
* sel - 1) select  0) deselect 
* */

/* legacy warning, this function combines too much :) */
static int select_linked_limited_invoke(ViewContext *vc, short all, short sel)
{
	EditMesh *em= vc->em;
	EditFace *efa;
	EditEdge *eed;
	EditVert *eve;
	short done=1, change=0;
	
	if(em->faces.first==0) return OPERATOR_CANCELLED;
	
	/* flag all edges+faces as off*/
	for(eed= em->edges.first; eed; eed= eed->next)
		eed->tmp.l=0;
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		efa->tmp.l = 0;
	}
	
	if (all) {
		// XXX verts?
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->f & SELECT)
				eed->tmp.l= 1;
		}
		for(efa= em->faces.first; efa; efa= efa->next) {
			
			if (efa->f & SELECT) {
				face_tag(efa);
			} else {
				efa->tmp.l = 0;
			}
		}
	} 
	else {
		if( unified_findnearest(vc, &eve, &eed, &efa) ) {
			
			if(efa) {
				efa->tmp.l = 1;
				face_tag(efa);
			}
			else if(eed)
				eed->tmp.l= 1;
			else {
				for(eed= em->edges.first; eed; eed= eed->next)
					if(eed->v1==eve || eed->v2==eve)
						break;
				eed->tmp.l= 1;
			}
		}
		else
			return OPERATOR_FINISHED;
	}
	
	while(done==1) {
		done= 0;
		/* simple algo - select all faces that have a selected edge
		* this intern selects the edge, repeat until nothing is left to do */
		for(efa= em->faces.first; efa; efa= efa->next) {
			if ((efa->tmp.l == 0) && (!efa->h)) {
				if (is_face_tag(efa)) {
					face_tag(efa);
					done= 1;
				}
			}
		}
	}
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		if (efa->tmp.l) {
			if (sel) {
				if (!(efa->f & SELECT)) {
					EM_select_face(efa, 1);
					change = 1;
				}
			} else {
				if (efa->f & SELECT) {
					EM_select_face(efa, 0);
					change = 1;
				}
			}
		}
	}
	
	if (!change)
		return OPERATOR_CANCELLED;
	
	if (!sel) /* make sure de-selecting faces didnt de-select the verts/edges connected to selected faces, this is common with boundries */
		for(efa= em->faces.first; efa; efa= efa->next)
			if (efa->f & SELECT)
				EM_select_face(efa, 1);
	
	//	if (EM_texFaceCheck())
	
	return OPERATOR_FINISHED;
}

#undef is_edge_delimit_ok
#undef is_face_tag
#undef face_tag

static void linked_limit_default(bContext *C, wmOperator *op) {
	if(!RNA_property_is_set(op->ptr, "limit")) {
		Object *obedit= CTX_data_edit_object(C);
		EditMesh *em= BKE_mesh_get_editmesh(obedit->data);
		if(em->selectmode == SCE_SELECT_FACE)
			RNA_boolean_set(op->ptr, "limit", TRUE);
	}
}

static int select_linked_pick_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Object *obedit= CTX_data_edit_object(C);
	ViewContext vc;
	EditVert *eve, *v1, *v2;
	EditEdge *eed;
	EditFace *efa;
	short done=1, toggle=0;
	int sel= !RNA_boolean_get(op->ptr, "deselect");
	int limit;
	
	linked_limit_default(C, op);

	limit = RNA_boolean_get(op->ptr, "limit");

	/* unified_finednearest needs ogl */
	view3d_operator_needs_opengl(C);
	
	/* setup view context for argument to callbacks */
	em_setup_viewcontext(C, &vc);
	
	if(vc.em->edges.first==0) return OPERATOR_CANCELLED;
	
	vc.mval[0]= event->mval[0];
	vc.mval[1]= event->mval[1];
	
	/* return warning! */
	if(limit) {
		int retval= select_linked_limited_invoke(&vc, 0, sel);
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
		return retval;
	}
	
	if( unified_findnearest(&vc, &eve, &eed, &efa)==0 ) {
		WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
	
		return OPERATOR_CANCELLED;
	}

	/* clear test flags */
	for(v1= vc.em->verts.first; v1; v1= v1->next) v1->f1= 0;
	
	/* start vertex/face/edge */
	if(eve) eve->f1= 1;
	else if(eed) eed->v1->f1= eed->v2->f1= 1;
	else efa->v1->f1= efa->v2->f1= efa->v3->f1= 1;
	
	/* set flag f1 if affected */
	while(done==1) {
		done= 0;
		toggle++;
		
		if(toggle & 1) eed= vc.em->edges.first;
		else eed= vc.em->edges.last;
		
		while(eed) {
			v1= eed->v1;
			v2= eed->v2;
			
			if(eed->h==0) {
				if(v1->f1 && v2->f1==0) {
					v2->f1= 1;
					done= 1;
				}
				else if(v1->f1==0 && v2->f1) {
					v1->f1= 1;
					done= 1;
				}
			}
			
			if(toggle & 1) eed= eed->next;
			else eed= eed->prev;
		}
	}
	
	/* now use vertex f1 flag to select/deselect */
	for(eed= vc.em->edges.first; eed; eed= eed->next) {
		if(eed->v1->f1 && eed->v2->f1) 
			EM_select_edge(eed, sel);
	}
	for(efa= vc.em->faces.first; efa; efa= efa->next) {
		if(efa->v1->f1 && efa->v2->f1 && efa->v3->f1 && (efa->v4==NULL || efa->v4->f1)) 
			EM_select_face(efa, sel);
	}
	/* no flush needed, connected geometry is done */
	
//	if (EM_texFaceCheck())
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
	return OPERATOR_FINISHED;	
}

void MESH_OT_select_linked_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Linked";
	ot->description= "(un)select all vertices linked to the active mesh";
	ot->idname= "MESH_OT_select_linked_pick";
	
	/* api callbacks */
	ot->invoke= select_linked_pick_invoke;
	ot->poll= ED_operator_editmesh_view3d;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "deselect", 0, "Deselect", "");
	RNA_def_boolean(ot->srna, "limit", 0, "Limit by Seams", "");
}


/* ************************* */

void selectconnected_mesh_all(EditMesh *em)
{
	EditVert *v1,*v2;
	EditEdge *eed;
	short done=1, toggle=0;
	
	if(em->edges.first==0) return;
	
	while(done==1) {
		done= 0;
		
		toggle++;
		if(toggle & 1) eed= em->edges.first;
		else eed= em->edges.last;
		
		while(eed) {
			v1= eed->v1;
			v2= eed->v2;
			if(eed->h==0) {
				if(v1->f & SELECT) {
					if( (v2->f & SELECT)==0 ) {
						v2->f |= SELECT;
						done= 1;
					}
				}
				else if(v2->f & SELECT) {
					if( (v1->f & SELECT)==0 ) {
						v1->f |= SELECT;
						done= 1;
					}
				}
			}
			if(toggle & 1) eed= eed->next;
			else eed= eed->prev;
		}
	}
	
	/* now use vertex select flag to select rest */
	EM_select_flush(em);
	
	//	if (EM_texFaceCheck())
}

static int select_linked_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(obedit->data);
	
	if( RNA_boolean_get(op->ptr, "limit") ) {
		ViewContext vc;
		em_setup_viewcontext(C, &vc);
		select_linked_limited_invoke(&vc, 1, 1);
	}
	else
		selectconnected_mesh_all(em);
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;	
}

static int select_linked_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	linked_limit_default(C, op);
	return select_linked_exec(C, op);
}

void MESH_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Linked All";
	ot->description= "Select all vertices linked to the active mesh";
	ot->idname= "MESH_OT_select_linked";
	
	/* api callbacks */
	ot->exec= select_linked_exec;
	ot->invoke= select_linked_invoke;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "limit", 0, "Limit by Seams", "");
}


/* ************************* */

/* swap is 0 or 1, if 1 it hides not selected */
void EM_hide_mesh(EditMesh *em, int swap)
{
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	int a;
	
	if(em==NULL) return;

	/* hide happens on least dominant select mode, and flushes up, not down! (helps preventing errors in subsurf) */
	/*  - vertex hidden, always means edge is hidden too
		- edge hidden, always means face is hidden too
		- face hidden, only set face hide
		- then only flush back down what's absolute hidden
	*/
	if(em->selectmode & SCE_SELECT_VERTEX) {
		for(eve= em->verts.first; eve; eve= eve->next) {
			if((eve->f & SELECT)!=swap) {
				eve->f &= ~SELECT;
				eve->h= 1;
			}
		}
	
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->v1->h || eed->v2->h) {
				eed->h |= 1;
				eed->f &= ~SELECT;
			}
		}
	
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->e1->h & 1 || efa->e2->h & 1 || efa->e3->h & 1 || (efa->e4 && efa->e4->h & 1)) {
				efa->h= 1;
				efa->f &= ~SELECT;
			}
		}
	}
	else if(em->selectmode & SCE_SELECT_EDGE) {

		for(eed= em->edges.first; eed; eed= eed->next) {
			if((eed->f & SELECT)!=swap) {
				eed->h |= 1;
				EM_select_edge(eed, 0);
			}
		}

		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->e1->h & 1 || efa->e2->h & 1 || efa->e3->h & 1 || (efa->e4 && efa->e4->h & 1)) {
				efa->h= 1;
				efa->f &= ~SELECT;
			}
		}
	}
	else {

		for(efa= em->faces.first; efa; efa= efa->next) {
			if((efa->f & SELECT)!=swap) {
				efa->h= 1;
				EM_select_face(efa, 0);
			}
		}
	}
	
	/* flush down, only whats 100% hidden */
	for(eve= em->verts.first; eve; eve= eve->next) eve->f1= 0;
	for(eed= em->edges.first; eed; eed= eed->next) eed->f1= 0;
	
	if(em->selectmode & SCE_SELECT_FACE) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->h) a= 1; else a= 2;
			efa->e1->f1 |= a;
			efa->e2->f1 |= a;
			efa->e3->f1 |= a;
			if(efa->e4) efa->e4->f1 |= a;
			/* When edges are not delt with in their own loop, we need to explicitly re-selct select edges that are joined to unselected faces */
			if (swap && (em->selectmode == SCE_SELECT_FACE) && (efa->f & SELECT)) {
				EM_select_face(efa, 1);
			}
		}
	}
	
	if(em->selectmode >= SCE_SELECT_EDGE) {
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->f1==1) eed->h |= 1;
			if(eed->h & 1) a= 1; else a= 2;
			eed->v1->f1 |= a;
			eed->v2->f1 |= a;
		}
	}

	if(em->selectmode >= SCE_SELECT_VERTEX) {
		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->f1==1) eve->h= 1;
		}
	}
	
	em->totedgesel= em->totfacesel= em->totvertsel= 0;
//	if(EM_texFaceCheck())

	//	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);	
}

static int hide_mesh_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	
	EM_hide_mesh(em, RNA_boolean_get(op->ptr, "unselected"));
		
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;	
}

void MESH_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Hide Selection";
	ot->description= "Hide (un)selected vertices, edges or faces";
	ot->idname= "MESH_OT_hide";
	
	/* api callbacks */
	ot->exec= hide_mesh_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected.");
}

void EM_reveal_mesh(EditMesh *em)
{
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	
	if(em==NULL) return;

	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->h) {
			eve->h= 0;
			eve->f |= SELECT;
		}
	}
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->h & 1) {
			eed->h &= ~1;
			if(em->selectmode & SCE_SELECT_VERTEX); 
			else EM_select_edge(eed, 1);
		}
	}
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->h) {
			efa->h= 0;
			if(em->selectmode & (SCE_SELECT_EDGE|SCE_SELECT_VERTEX)); 
			else EM_select_face(efa, 1);
		}
	}

	EM_fgon_flags(em);	/* redo flags and indices for fgons */
	EM_selectmode_flush(em);

//	if (EM_texFaceCheck())
//	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);	
}

static int reveal_mesh_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	
	EM_reveal_mesh(em);

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;	
}

void MESH_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reveal Hidden";
	ot->description= "Reveal all hidden vertices, edges and faces";
	ot->idname= "MESH_OT_reveal";
	
	/* api callbacks */
	ot->exec= reveal_mesh_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

int select_by_number_vertices_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	EditFace *efa;
	int numverts= RNA_enum_get(op->ptr, "type");

	/* Selects trias/qiads or isolated verts, and edges that do not have 2 neighboring
	 * faces
	 */

	/* for loose vertices/edges, we first select all, loop below will deselect */
	if(numverts==5) {
		EM_set_flag_all(em, SELECT);
	}
	else if(em->selectmode!=SCE_SELECT_FACE) {
		BKE_report(op->reports, RPT_ERROR, "Only works in face selection mode");
		return OPERATOR_CANCELLED;
	}
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		if (efa->e4) {
			EM_select_face(efa, (numverts==4) );
		}
		else {
			EM_select_face(efa, (numverts==3) );
		}
	}
	
	EM_selectmode_flush(em);

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
	
	return OPERATOR_FINISHED;
}

void MESH_OT_select_by_number_vertices(wmOperatorType *ot)
{
	static const EnumPropertyItem type_items[]= {
		{3, "TRIANGLES", 0, "Triangles", NULL},
		{4, "QUADS", 0, "Quads", NULL},
		{5, "OTHER", 0, "Other", NULL},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Select by Number of Vertices";
	ot->description= "Select vertices or faces by vertex count";
	ot->idname= "MESH_OT_select_by_number_vertices";
	
	/* api callbacks */
	ot->exec= select_by_number_vertices_exec;
	ot->invoke= WM_menu_invoke;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	ot->prop= RNA_def_enum(ot->srna, "type", type_items, 3, "Type", "Type of elements to select.");
}


int select_mirror_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));

	int extend= RNA_boolean_get(op->ptr, "extend");

	EM_select_mirrored(obedit, em, extend);
	EM_selectmode_flush(em);
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Mirror";
	ot->description= "Select mesh items at mirrored locations";
	ot->idname= "MESH_OT_select_mirror";

	/* api callbacks */
	ot->exec= select_mirror_exec;
	ot->poll= ED_operator_editmesh;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the existing selection");
}

static int select_sharp_edges_exec(bContext *C, wmOperator *op)
{
	/* Find edges that have exactly two neighboring faces,
	* check the angle between those faces, and if angle is
	* small enough, select the edge
	*/
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	EditEdge *eed;
	EditFace *efa;
	EditFace **efa1;
	EditFace **efa2;
	intptr_t edgecount = 0, i = 0;
	float sharpness, fsharpness;
	
	/* 'standard' behaviour - check if selected, then apply relevant selection */
	
	if(em->selectmode==SCE_SELECT_FACE) {
		BKE_report(op->reports, RPT_ERROR, "Doesn't work in face selection mode");
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;
	}

	sharpness= RNA_float_get(op->ptr, "sharpness");
	fsharpness = ((180.0 - sharpness) * M_PI) / 180.0;

	/* count edges, use tmp.l  */
	eed= em->edges.first;
	while(eed) {
		edgecount++;
		eed->tmp.l = i;
		eed= eed->next;
		++i;
	}

	/* for each edge, we want a pointer to two adjacent faces */
	efa1 = MEM_callocN(edgecount*sizeof(EditFace *), 
					   "pairs of edit face pointers");
	efa2 = MEM_callocN(edgecount*sizeof(EditFace *), 
					   "pairs of edit face pointers");

#define face_table_edge(eed) { \
		i = eed->tmp.l; \
		if (i != -1) { \
			if (efa1[i]) { \
				if (efa2[i]) { \
					/* invalidate, edge has more than two neighbors */ \
					eed->tmp.l = -1; \
				} \
				else { \
					efa2[i] = efa; \
				} \
			} \
			else { \
				efa1[i] = efa; \
			} \
		} \
	}

	/* find the adjacent faces of each edge, we want only two */
	efa= em->faces.first;
	while(efa) {
		face_table_edge(efa->e1);
		face_table_edge(efa->e2);
		face_table_edge(efa->e3);
		if (efa->e4) {
			face_table_edge(efa->e4);
		}
		efa= efa->next;
	}

#undef face_table_edge

	eed = em->edges.first;
	while(eed) {
		i = eed->tmp.l;
		if (i != -1) { 
			/* edge has two or less neighboring faces */
			if ( (efa1[i]) && (efa2[i]) ) { 
				/* edge has exactly two neighboring faces, check angle */
				float angle;
				angle = saacos(efa1[i]->n[0]*efa2[i]->n[0] +
							   efa1[i]->n[1]*efa2[i]->n[1] +
							   efa1[i]->n[2]*efa2[i]->n[2]);
				if (fabs(angle) >= fsharpness)
					EM_select_edge(eed, 1);
			}
		}

		eed= eed->next;
	}

	MEM_freeN(efa1);
	MEM_freeN(efa2);

//	if (EM_texFaceCheck())
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data); //TODO is this needed ?

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;	
}

void MESH_OT_edges_select_sharp(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Sharp Edges";
	ot->description= "Marked selected edges as sharp";
	ot->idname= "MESH_OT_edges_select_sharp";
	
	/* api callbacks */
	ot->exec= select_sharp_edges_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_float(ot->srna, "sharpness", 0.01f, 0.0f, FLT_MAX, "sharpness", "", 0.0f, 180.0f);
}


static void select_linked_flat_faces(EditMesh *em, wmOperator *op, float sharpness)
{
	/* Find faces that are linked to selected faces that are 
	 * relatively flat (angle between faces is higher than
	 * specified angle)
	 */
	EditEdge *eed;
	EditFace *efa;
	EditFace **efa1;
	EditFace **efa2;
	intptr_t edgecount = 0, i, faceselcount=0, faceselcountold=0;
	float fsharpness;
	
	if(em->selectmode!=SCE_SELECT_FACE) {
		BKE_report(op->reports, RPT_ERROR, "Only works in face selection mode");
		return;
	}

	fsharpness = ((180.0 - sharpness) * M_PI) / 180.0;

	i=0;
	/* count edges, use tmp.l */
	eed= em->edges.first;
	while(eed) {
		edgecount++;
		eed->tmp.l = i;
		eed= eed->next;
		++i;
	}

	/* for each edge, we want a pointer to two adjacent faces */
	efa1 = MEM_callocN(edgecount*sizeof(EditFace *), 
					   "pairs of edit face pointers");
	efa2 = MEM_callocN(edgecount*sizeof(EditFace *), 
					   "pairs of edit face pointers");

#define face_table_edge(eed) { \
		i = eed->tmp.l; \
		if (i != -1) { \
			if (efa1[i]) { \
				if (efa2[i]) { \
					/* invalidate, edge has more than two neighbors */ \
					eed->tmp.l = -1; \
				} \
				else { \
					efa2[i] = efa; \
				} \
			} \
			else { \
				efa1[i] = efa; \
			} \
		} \
	}

	/* find the adjacent faces of each edge, we want only two */
	efa= em->faces.first;
	while(efa) {
		face_table_edge(efa->e1);
		face_table_edge(efa->e2);
		face_table_edge(efa->e3);
		if (efa->e4) {
			face_table_edge(efa->e4);
		}

		/* while were at it, count the selected faces */
		if (efa->f & SELECT) ++faceselcount;

		efa= efa->next;
	}

#undef face_table_edge

	eed= em->edges.first;
	while(eed) {
		i = eed->tmp.l;
		if (i != -1) { 
			/* edge has two or less neighboring faces */
			if ( (efa1[i]) && (efa2[i]) ) { 
				/* edge has exactly two neighboring faces, check angle */
				float angle;
				angle = saacos(efa1[i]->n[0]*efa2[i]->n[0] +
							   efa1[i]->n[1]*efa2[i]->n[1] +
							   efa1[i]->n[2]*efa2[i]->n[2]);
				/* invalidate: edge too sharp */
				if (fabs(angle) >= fsharpness)
					eed->tmp.l = -1;
			}
			else {
				/* invalidate: less than two neighbors */
				eed->tmp.l = -1;
			}
		}

		eed= eed->next;
	}

#define select_flat_neighbor(eed) { \
				i = eed->tmp.l; \
				if (i!=-1) { \
					if (! (efa1[i]->f & SELECT) ) { \
						EM_select_face(efa1[i], 1); \
						++faceselcount; \
					} \
					if (! (efa2[i]->f & SELECT) ) { \
						EM_select_face(efa2[i], 1); \
						++faceselcount; \
					} \
				} \
	}

	while (faceselcount != faceselcountold) {
		faceselcountold = faceselcount;

		efa= em->faces.first;
		while(efa) {
			if (efa->f & SELECT) {
				select_flat_neighbor(efa->e1);
				select_flat_neighbor(efa->e2);
				select_flat_neighbor(efa->e3);
				if (efa->e4) {
					select_flat_neighbor(efa->e4);
				}
			}
			efa= efa->next;
		}
	}

#undef select_flat_neighbor

	MEM_freeN(efa1);
	MEM_freeN(efa2);

//	if (EM_texFaceCheck())

}

static int select_linked_flat_faces_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	
	select_linked_flat_faces(em, op, RNA_float_get(op->ptr, "sharpness"));
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;	
}

void MESH_OT_faces_select_linked_flat(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Linked Flat Faces";
	ot->description= "Select linked faces by angle";
	ot->idname= "MESH_OT_faces_select_linked_flat";
	
	/* api callbacks */
	ot->exec= select_linked_flat_faces_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_float(ot->srna, "sharpness", 0.0f, 0.0f, FLT_MAX, "sharpness", "", 0.0f, 180.0f);
}

void select_non_manifold(EditMesh *em, wmOperator *op )
{
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;

	/* Selects isolated verts, and edges that do not have 2 neighboring
	 * faces
	 */
	
	if(em->selectmode==SCE_SELECT_FACE) {
		BKE_report(op->reports, RPT_ERROR, "Doesn't work in face selection mode");
		return;
	}

	eve= em->verts.first;
	while(eve) {
		/* this will count how many edges are connected
		 * to this vert */
		eve->f1= 0;
		eve= eve->next;
	}

	eed= em->edges.first;
	while(eed) {
		/* this will count how many faces are connected to
		 * this edge */
		eed->f1= 0;
		/* increase edge count for verts */
		++eed->v1->f1;
		++eed->v2->f1;
		eed= eed->next;
	}

	efa= em->faces.first;
	while(efa) {
		/* increase face count for edges */
		++efa->e1->f1;
		++efa->e2->f1;
		++efa->e3->f1;
		if (efa->e4)
			++efa->e4->f1;			
		efa= efa->next;
	}

	/* select verts that are attached to an edge that does not
	 * have 2 neighboring faces */
	eed= em->edges.first;
	while(eed) {
		if (eed->h==0 && eed->f1 != 2) {
			EM_select_edge(eed, 1);
		}
		eed= eed->next;
	}

	/* select isolated verts */
	if(em->selectmode & SCE_SELECT_VERTEX) {
		eve= em->verts.first;
		while(eve) {
			if (eve->f1 == 0) {
				if (!eve->h) eve->f |= SELECT;
			}
			eve= eve->next;
		}
	}

//	if (EM_texFaceCheck())

}

static int select_non_manifold_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	
	select_non_manifold(em, op);
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;	
}

void MESH_OT_select_non_manifold(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Non Manifold";
	ot->description= "Select all non-manifold vertices or edges";
	ot->idname= "MESH_OT_select_non_manifold";
	
	/* api callbacks */
	ot->exec= select_non_manifold_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

void EM_select_swap(EditMesh *em) /* exported for UV */
{
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	
	if(em->selectmode & SCE_SELECT_VERTEX) {

		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->h==0) {
				if(eve->f & SELECT) eve->f &= ~SELECT;
				else eve->f|= SELECT;
			}
		}
	}
	else if(em->selectmode & SCE_SELECT_EDGE) {
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->h==0) {
				EM_select_edge(eed, !(eed->f & SELECT));
			}
		}
	}
	else {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->h==0) {
				EM_select_face(efa, !(efa->f & SELECT));
			}
		}
	}

	EM_selectmode_flush(em);
	
//	if (EM_texFaceCheck())

}

static int select_inverse_mesh_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	
	EM_select_swap(em);
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;	
}

void MESH_OT_select_inverse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Inverse";
	ot->description= "Select inverse of (un)selected vertices, edges or faces";
	ot->idname= "MESH_OT_select_inverse";
	
	/* api callbacks */
	ot->exec= select_inverse_mesh_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
	
/* ******************** (de)select all operator **************** */

void EM_toggle_select_all(EditMesh *em) /* exported for UV */
{
	if(EM_nvertices_selected(em))
		EM_clear_flag_all(em, SELECT);
	else 
		EM_set_flag_all_selectmode(em, SELECT);
}

void EM_select_all(EditMesh *em)
{
	EM_set_flag_all_selectmode(em, SELECT);
}

void EM_deselect_all(EditMesh *em)
{
	EM_clear_flag_all(em, SELECT);
}

static int select_all_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	int action = RNA_enum_get(op->ptr, "action");
	
	switch (action) {
	case SEL_TOGGLE:
		EM_toggle_select_all(em);
		break;
	case SEL_SELECT:
		EM_select_all(em);
		break;
	case SEL_DESELECT:
		EM_deselect_all(em);
		break;
	case SEL_INVERT:
		EM_select_swap(em);
		break;
	}
	
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);	
	BKE_mesh_end_editmesh(obedit->data, em);

	return OPERATOR_FINISHED;
}

void MESH_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select/Deselect All";
	ot->description= "Change selection of all vertices, edges or faces";
	ot->idname= "MESH_OT_select_all";
	
	/* api callbacks */
	ot->exec= select_all_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

/* ******************** **************** */

void EM_select_more(EditMesh *em)
{
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->f & SELECT) eve->f1= 1;
		else eve->f1 = 0;
	}
	
	/* set f1 flags in vertices to select 'more' */
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->h==0) {
			if (eed->v1->f & SELECT)
				eed->v2->f1 = 1;
			if (eed->v2->f & SELECT)
				eed->v1->f1 = 1;
		}
	}

	/* new selected edges, but not in facemode */
	if(em->selectmode <= SCE_SELECT_EDGE) {
		
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->h==0) {
				if(eed->v1->f1 && eed->v2->f1) EM_select_edge(eed, 1);
			}
		}
	}
	/* new selected faces */
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->h==0) {
			if(efa->v1->f1 && efa->v2->f1 && efa->v3->f1 && (efa->v4==NULL || efa->v4->f1)) 
				EM_select_face(efa, 1);
		}
	}
}

static int select_more(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data)) ;

	EM_select_more(em);

//	if (EM_texFaceCheck(em))

	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select More";
	ot->description= "Select more vertices, edges or faces connected to initial selection";
	ot->idname= "MESH_OT_select_more";

	/* api callbacks */
	ot->exec= select_more;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

void EM_select_less(EditMesh *em)
{
	EditEdge *eed;
	EditFace *efa;

	if(em->selectmode <= SCE_SELECT_EDGE) {
		/* eed->f1 == 1:  edge with a selected and deselected vert */ 

		for(eed= em->edges.first; eed; eed= eed->next) {
			eed->f1= 0;
			if(eed->h==0) {
				
				if ( !(eed->v1->f & SELECT) && (eed->v2->f & SELECT) ) 
					eed->f1= 1;
				if ( (eed->v1->f & SELECT) && !(eed->v2->f & SELECT) ) 
					eed->f1= 1;
			}
		}
		
		/* deselect edges with flag set */
		for(eed= em->edges.first; eed; eed= eed->next) {
			if (eed->h==0 && eed->f1 == 1) {
				EM_select_edge(eed, 0);
			}
		}
		EM_deselect_flush(em);
		
	}
	else {
		/* deselect faces with 1 or more deselect edges */
		/* eed->f1 == mixed selection edge */
		for(eed= em->edges.first; eed; eed= eed->next) eed->f1= 0;

		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->h==0) {
				if(efa->f & SELECT) {
					efa->e1->f1 |= 1;
					efa->e2->f1 |= 1;
					efa->e3->f1 |= 1;
					if(efa->e4) efa->e4->f1 |= 1;
				}
				else {
					efa->e1->f1 |= 2;
					efa->e2->f1 |= 2;
					efa->e3->f1 |= 2;
					if(efa->e4) efa->e4->f1 |= 2;
				}
			}
		}
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->h==0) {
				if(efa->e1->f1==3 || efa->e2->f1==3 || efa->e3->f1==3 || (efa->e4 && efa->e4->f1==3)) { 
					EM_select_face(efa, 0);
				}
			}
		}
		EM_selectmode_flush(em);
		
	}
}

static int select_less(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));

	EM_select_less(em);

//	if (EM_texFaceCheck(em))
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);

	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;
}

void MESH_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Less";
	ot->description= "Select less vertices, edges or faces connected to initial selection";
	ot->idname= "MESH_OT_select_less";

	/* api callbacks */
	ot->exec= select_less;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static void selectrandom_mesh(EditMesh *em, float randfac) /* randomly selects a user-set % of vertices/edges/faces */
{
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;

	BLI_srand( BLI_rand() ); /* random seed */
	
	if(em->selectmode & SCE_SELECT_VERTEX) {
		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->h==0) {
				if (BLI_frand() < randfac) 
					eve->f |= SELECT;
			}
		}
		EM_selectmode_flush(em);
	}
	else if(em->selectmode & SCE_SELECT_EDGE) {
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->h==0) {
				if (BLI_frand() < randfac) 
					EM_select_edge(eed, 1);
			}
		}
		EM_selectmode_flush(em);
	}
	else {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->h==0) {
				if (BLI_frand() < randfac) 
					EM_select_face(efa, 1);
			}
		}
		
		EM_selectmode_flush(em);
	}
//	if (EM_texFaceCheck())
}

static int mesh_select_random_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	
	if(!RNA_boolean_get(op->ptr, "extend"))
		EM_deselect_all(em);
	
	selectrandom_mesh(em, RNA_float_get(op->ptr, "percent")/100.0f);
		
	WM_event_add_notifier(C, NC_GEOM|ND_SELECT, obedit->data);
	
	BKE_mesh_end_editmesh(obedit->data, em);
	return OPERATOR_FINISHED;	
}

void MESH_OT_select_random(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Random";
	ot->description= "Randomly select vertices";
	ot->idname= "MESH_OT_select_random";

	/* api callbacks */
	ot->exec= mesh_select_random_exec;
	ot->poll= ED_operator_editmesh;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_float_percentage(ot->srna, "percent", 50.f, 0.0f, 100.0f, "Percent", "Percentage of elements to select randomly.", 0.f, 100.0f);
	RNA_def_boolean(ot->srna, "extend", FALSE, "Extend Selection", "Extend selection instead of deselecting everything first.");
}

void EM_select_by_material(EditMesh *em, int index) 
{
	EditFace *efa;
	
	for (efa=em->faces.first; efa; efa= efa->next) {
		if (efa->mat_nr==index) {
			EM_select_face(efa, 1);
		}
	}

	EM_selectmode_flush(em);
}

void EM_deselect_by_material(EditMesh *em, int index) 
{
	EditFace *efa;
	
	for (efa=em->faces.first; efa; efa= efa->next) {
		if (efa->mat_nr==index) {
			EM_select_face(efa, 0);
		}
	}

	EM_selectmode_flush(em);
}

/* ************************* SEAMS AND EDGES **************** */

static int editmesh_mark_seam(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	Mesh *me= ((Mesh *)obedit->data);
	EditEdge *eed;
	int clear = RNA_boolean_get(op->ptr, "clear");
	
	/* auto-enable seams drawing */
	if(clear==0) {
		me->drawflag |= ME_DRAWSEAMS;
	}

	if(clear) {
		eed= em->edges.first;
		while(eed) {
			if((eed->h==0) && (eed->f & SELECT)) {
				eed->seam = 0;
			}
			eed= eed->next;
		}
	}
	else {
		eed= em->edges.first;
		while(eed) {
			if((eed->h==0) && (eed->f & SELECT)) {
				eed->seam = 1;
			}
			eed= eed->next;
		}
	}

	BKE_mesh_end_editmesh(obedit->data, em);

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_mark_seam(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mark Seam";
	ot->description= "(un)mark selected edges as a seam";
	ot->idname= "MESH_OT_mark_seam";
	
	/* api callbacks */
	ot->exec= editmesh_mark_seam;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "clear", 0, "Clear", "");
}

static int editmesh_mark_sharp(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	Mesh *me= ((Mesh *)obedit->data);
	int clear = RNA_boolean_get(op->ptr, "clear");
	EditEdge *eed;

	/* auto-enable sharp edge drawing */
	if(clear == 0) {
		me->drawflag |= ME_DRAWSHARP;
	}

	if(!clear) {
		eed= em->edges.first;
		while(eed) {
			if(!eed->h && (eed->f & SELECT)) eed->sharp = 1;
			eed = eed->next;
		}
	} else {
		eed= em->edges.first;
		while(eed) {
			if(!eed->h && (eed->f & SELECT)) eed->sharp = 0;
			eed = eed->next;
		}
	}

	BKE_mesh_end_editmesh(obedit->data, em);

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_mark_sharp(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mark Sharp";
	ot->description= "(un)mark selected edges as sharp";
	ot->idname= "MESH_OT_mark_sharp";
	
	/* api callbacks */
	ot->exec= editmesh_mark_sharp;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "clear", 0, "Clear", "");
}

/* **************** NORMALS ************** */

void EM_recalc_normal_direction(EditMesh *em, int inside, int select)	/* makes faces righthand turning */
{
	EditEdge *eed, *ed1, *ed2, *ed3, *ed4;
	EditFace *efa, *startvl;
	float maxx, nor[3], cent[3];
	int totsel, found, foundone, direct, turn, tria_nr;

   /* based at a select-connected to witness loose objects */

	/* count per edge the amount of faces */

	/* find the ultimate left, front, upper face (not manhattan dist!!) */
	/* also evaluate both triangle cases in quad, since these can be non-flat */

	/* put normal to the outside, and set the first direction flags in edges */

	/* then check the object, and set directions / direction-flags: but only for edges with 1 or 2 faces */
	/* this is in fact the 'select connected' */
	
	/* in case (selected) faces were not done: start over with 'find the ultimate ...' */

	waitcursor(1);
	
	eed= em->edges.first;
	while(eed) {
		eed->f2= 0;		/* edge direction */
		eed->f1= 0;		/* counter */
		eed= eed->next;
	}

	/* count faces and edges */
	totsel= 0;
	efa= em->faces.first;
	while(efa) {
		if(select==0 || (efa->f & SELECT) ) {
			efa->f1= 1;
			totsel++;
			efa->e1->f1++;
			efa->e2->f1++;
			efa->e3->f1++;
			if(efa->v4) efa->e4->f1++;
		}
		else efa->f1= 0;

		efa= efa->next;
	}

	while(totsel>0) {
		/* from the outside to the inside */

		efa= em->faces.first;
		startvl= NULL;
		maxx= -1.0e10;
		tria_nr= 0;

		while(efa) {
			if(efa->f1) {
				cent_tri_v3(cent, efa->v1->co, efa->v2->co, efa->v3->co);
				cent[0]= cent[0]*cent[0] + cent[1]*cent[1] + cent[2]*cent[2];
				
				if(cent[0]>maxx) {
					maxx= cent[0];
					startvl= efa;
					tria_nr= 0;
				}
				if(efa->v4) {
					cent_tri_v3(cent, efa->v1->co, efa->v3->co, efa->v4->co);
					cent[0]= cent[0]*cent[0] + cent[1]*cent[1] + cent[2]*cent[2];
					
					if(cent[0]>maxx) {
						maxx= cent[0];
						startvl= efa;
						tria_nr= 1;
					}
				}
			}
			efa= efa->next;
		}

		if (startvl==NULL)
			startvl= em->faces.first;
		
		/* set first face correct: calc normal */
		
		if(tria_nr==1) {
			normal_tri_v3( nor,startvl->v1->co, startvl->v3->co, startvl->v4->co);
			cent_tri_v3(cent, startvl->v1->co, startvl->v3->co, startvl->v4->co);
		} else {
			normal_tri_v3( nor,startvl->v1->co, startvl->v2->co, startvl->v3->co);
			cent_tri_v3(cent, startvl->v1->co, startvl->v2->co, startvl->v3->co);
		}
		/* first normal is oriented this way or the other */
		if(inside) {
			if(cent[0]*nor[0]+cent[1]*nor[1]+cent[2]*nor[2] > 0.0) flipface(em, startvl);
		}
		else {
			if(cent[0]*nor[0]+cent[1]*nor[1]+cent[2]*nor[2] < 0.0) flipface(em, startvl);
		}

		eed= startvl->e1;
		if(eed->v1==startvl->v1) eed->f2= 1; 
		else eed->f2= 2;
		
		eed= startvl->e2;
		if(eed->v1==startvl->v2) eed->f2= 1; 
		else eed->f2= 2;
		
		eed= startvl->e3;
		if(eed->v1==startvl->v3) eed->f2= 1; 
		else eed->f2= 2;
		
		eed= startvl->e4;
		if(eed) {
			if(eed->v1==startvl->v4) eed->f2= 1; 
			else eed->f2= 2;
		}
		
		startvl->f1= 0;
		totsel--;

		/* test normals */
		found= 1;
		direct= 1;
		while(found) {
			found= 0;
			if(direct) efa= em->faces.first;
			else efa= em->faces.last;
			while(efa) {
				if(efa->f1) {
					turn= 0;
					foundone= 0;

					ed1= efa->e1;
					ed2= efa->e2;
					ed3= efa->e3;
					ed4= efa->e4;

					if(ed1->f2) {
						if(ed1->v1==efa->v1 && ed1->f2==1) turn= 1;
						if(ed1->v2==efa->v1 && ed1->f2==2) turn= 1;
						foundone= 1;
					}
					else if(ed2->f2) {
						if(ed2->v1==efa->v2 && ed2->f2==1) turn= 1;
						if(ed2->v2==efa->v2 && ed2->f2==2) turn= 1;
						foundone= 1;
					}
					else if(ed3->f2) {
						if(ed3->v1==efa->v3 && ed3->f2==1) turn= 1;
						if(ed3->v2==efa->v3 && ed3->f2==2) turn= 1;
						foundone= 1;
					}
					else if(ed4 && ed4->f2) {
						if(ed4->v1==efa->v4 && ed4->f2==1) turn= 1;
						if(ed4->v2==efa->v4 && ed4->f2==2) turn= 1;
						foundone= 1;
					}

					if(foundone) {
						found= 1;
						totsel--;
						efa->f1= 0;

						if(turn) {
							if(ed1->v1==efa->v1) ed1->f2= 2; 
							else ed1->f2= 1;
							if(ed2->v1==efa->v2) ed2->f2= 2; 
							else ed2->f2= 1;
							if(ed3->v1==efa->v3) ed3->f2= 2; 
							else ed3->f2= 1;
							if(ed4) {
								if(ed4->v1==efa->v4) ed4->f2= 2; 
								else ed4->f2= 1;
							}

							flipface(em, efa);

						}
						else {
							if(ed1->v1== efa->v1) ed1->f2= 1; 
							else ed1->f2= 2;
							if(ed2->v1==efa->v2) ed2->f2= 1; 
							else ed2->f2= 2;
							if(ed3->v1==efa->v3) ed3->f2= 1; 
							else ed3->f2= 2;
							if(ed4) {
								if(ed4->v1==efa->v4) ed4->f2= 1; 
								else ed4->f2= 2;
							}
						}
					}
				}
				if(direct) efa= efa->next;
				else efa= efa->prev;
			}
			direct= 1-direct;
		}
	}

	recalc_editnormals(em);
	
//	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);

	waitcursor(0);
}


static int normals_make_consistent_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	
	/* 'standard' behaviour - check if selected, then apply relevant selection */
	
	// XXX  need other args
	EM_recalc_normal_direction(em, RNA_boolean_get(op->ptr, "inside"), 1);
	
	BKE_mesh_end_editmesh(obedit->data, em);

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data); //TODO is this needed ?

	return OPERATOR_FINISHED;	
}

void MESH_OT_normals_make_consistent(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Make Normals Consistent";
	ot->description= "Flip all selected vertex and face normals in a consistent direction";
	ot->idname= "MESH_OT_normals_make_consistent";
	
	/* api callbacks */
	ot->exec= normals_make_consistent_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "inside", 0, "Inside", "");
}

/* ********** ALIGN WITH VIEW **************** */


static void editmesh_calc_selvert_center(EditMesh *em, float cent_r[3])
{
	EditVert *eve;
	int nsel= 0;

	zero_v3(cent_r);

	for (eve= em->verts.first; eve; eve= eve->next) {
		if (eve->f & SELECT) {
			cent_r[0]+= eve->co[0];
			cent_r[1]+= eve->co[1];
			cent_r[2]+= eve->co[2];
			nsel++;
		}
	}

	if (nsel) {
		cent_r[0]/= nsel;
		cent_r[1]/= nsel;
		cent_r[2]/= nsel;
	}
}

static int mface_is_selected(MFace *mf)
{
	return (!(mf->flag & ME_HIDE) && (mf->flag & ME_FACE_SEL));
}

	/* XXX, code for both these functions should be abstract,
	 * then unified, then written for other things (like objects,
	 * which would use same as vertices method), then added
	 * to interface! Hoera! - zr
	 */
void faceselect_align_view_to_selected(View3D *v3d, RegionView3D *rv3d, Mesh *me, wmOperator *op,  int axis)
{
	float norm[3];
	int i, totselected = 0;

	norm[0]= norm[1]= norm[2]= 0.0;
	for (i=0; i<me->totface; i++) {
		MFace *mf= ((MFace*) me->mface) + i;

		if (mface_is_selected(mf)) {
			float *v1, *v2, *v3, fno[3];

			v1= me->mvert[mf->v1].co;
			v2= me->mvert[mf->v2].co;
			v3= me->mvert[mf->v3].co;
			if (mf->v4) {
				float *v4= me->mvert[mf->v4].co;
				normal_quad_v3( fno,v1, v2, v3, v4);
			} else {
				normal_tri_v3( fno,v1, v2, v3);
			}

			norm[0]+= fno[0];
			norm[1]+= fno[1];
			norm[2]+= fno[2];

			totselected++;
		}
	}

	if (totselected == 0)
		BKE_report(op->reports, RPT_ERROR, "No faces selected.");
	else
		view3d_align_axis_to_vector(v3d, rv3d, axis, norm);
}

/* helper for below, to survive non-uniform scaled objects */
static void face_getnormal_obspace(Object *obedit, EditFace *efa, float *fno)
{
	float vec[4][3];
	
	VECCOPY(vec[0], efa->v1->co);
	mul_mat3_m4_v3(obedit->obmat, vec[0]);
	VECCOPY(vec[1], efa->v2->co);
	mul_mat3_m4_v3(obedit->obmat, vec[1]);
	VECCOPY(vec[2], efa->v3->co);
	mul_mat3_m4_v3(obedit->obmat, vec[2]);
	if(efa->v4) {
		VECCOPY(vec[3], efa->v4->co);
		mul_mat3_m4_v3(obedit->obmat, vec[3]);
		
		normal_quad_v3( fno,vec[0], vec[1], vec[2], vec[3]);
	}
	else normal_tri_v3( fno,vec[0], vec[1], vec[2]);
}


void editmesh_align_view_to_selected(Object *obedit, EditMesh *em, wmOperator *op, View3D *v3d, RegionView3D *rv3d, int axis)
{
	int nselverts= EM_nvertices_selected(em);
	float norm[3]={0.0, 0.0, 0.0}; /* used for storing the mesh normal */
	
	if (nselverts==0) {
		BKE_report(op->reports, RPT_ERROR, "No faces or vertices selected.");
	} 
	else if (EM_nfaces_selected(em)) {
		EditFace *efa;
		for (efa= em->faces.first; efa; efa= efa->next) {
			if (faceselectedAND(efa, SELECT)) {
				float fno[3];
				
				face_getnormal_obspace(obedit, efa, fno);
				norm[0]+= fno[0];
				norm[1]+= fno[1];
				norm[2]+= fno[2];
			}
		}

		view3d_align_axis_to_vector(v3d, rv3d, axis, norm);
	} 
	else if (nselverts>2) {
		float cent[3];
		EditVert *eve, *leve= NULL;

		editmesh_calc_selvert_center(em, cent);
		for (eve= em->verts.first; eve; eve= eve->next) {
			if (eve->f & SELECT) {
				if (leve) {
					float tno[3];
					normal_tri_v3( tno,cent, leve->co, eve->co);
					
						/* XXX, fixme, should be flipped intp a 
						 * consistent direction. -zr
						 */
					norm[0]+= tno[0];
					norm[1]+= tno[1];
					norm[2]+= tno[2];
				}
				leve= eve;
			}
		}

		mul_mat3_m4_v3(obedit->obmat, norm);
		view3d_align_axis_to_vector(v3d, rv3d, axis, norm);
	} 
	else if (nselverts==2) { /* Align view to edge (or 2 verts) */ 
		EditVert *eve, *leve= NULL;

		for (eve= em->verts.first; eve; eve= eve->next) {
			if (eve->f & SELECT) {
				if (leve) {
					norm[0]= leve->co[0] - eve->co[0];
					norm[1]= leve->co[1] - eve->co[1];
					norm[2]= leve->co[2] - eve->co[2];
					break; /* we know there are only 2 verts so no need to keep looking */
				}
				leve= eve;
			}
		}
		mul_mat3_m4_v3(obedit->obmat, norm);
		view3d_align_axis_to_vector(v3d, rv3d, axis, norm);
	} 
	else if (nselverts==1) { /* Align view to vert normal */ 
		EditVert *eve;

		for (eve= em->verts.first; eve; eve= eve->next) {
			if (eve->f & SELECT) {
				norm[0]= eve->no[0];
				norm[1]= eve->no[1];
				norm[2]= eve->no[2];
				break; /* we know this is the only selected vert, so no need to keep looking */
			}
		}
		mul_mat3_m4_v3(obedit->obmat, norm);
		view3d_align_axis_to_vector(v3d, rv3d, axis, norm);
	}
} 

/* **************** VERTEX DEFORMS *************** */

static int smooth_vertex(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	EditVert *eve, *eve_mir = NULL;
	EditEdge *eed;
	float *adror, *adr, fac;
	float fvec[3];
	int teller=0;
	ModifierData *md;
	int index;

	/* count */
	eve= em->verts.first;
	while(eve) {
		if(eve->f & SELECT) teller++;
		eve= eve->next;
	}
	if(teller==0) {
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;
	}
	
	adr=adror= (float *)MEM_callocN(3*sizeof(float *)*teller, "vertsmooth");
	eve= em->verts.first;
	while(eve) {
		if(eve->f & SELECT) {
			eve->tmp.p = (void*)adr;
			eve->f1= 0;
			eve->f2= 0;
			adr+= 3;
		}
		eve= eve->next;
	}

	/* if there is a mirror modifier with clipping, flag the verts that
	 * are within tolerance of the plane(s) of reflection 
	 */
	for(md=obedit->modifiers.first; md; md=md->next) {
		if(md->type==eModifierType_Mirror) {
			MirrorModifierData *mmd = (MirrorModifierData*) md;	
		
			if(mmd->flag & MOD_MIR_CLIPPING) {
				for (eve= em->verts.first; eve; eve= eve->next) {
					if(eve->f & SELECT) {

						switch(mmd->axis){
							case 0:
								if (fabs(eve->co[0]) < mmd->tolerance)
									eve->f2 |= 1;
								break;
							case 1:
								if (fabs(eve->co[1]) < mmd->tolerance)
									eve->f2 |= 2;
								break;
							case 2:
								if (fabs(eve->co[2]) < mmd->tolerance)
									eve->f2 |= 4;
								break;
						}
					}
				}
			}
		}
	}
	
	eed= em->edges.first;
	while(eed) {
		if( (eed->v1->f & SELECT) || (eed->v2->f & SELECT) ) {
			fvec[0]= (eed->v1->co[0]+eed->v2->co[0])/2.0;
			fvec[1]= (eed->v1->co[1]+eed->v2->co[1])/2.0;
			fvec[2]= (eed->v1->co[2]+eed->v2->co[2])/2.0;
			
			if((eed->v1->f & SELECT) && eed->v1->f1<255) {
				eed->v1->f1++;
				add_v3_v3(eed->v1->tmp.p, fvec);
			}
			if((eed->v2->f & SELECT) && eed->v2->f1<255) {
				eed->v2->f1++;
				add_v3_v3(eed->v2->tmp.p, fvec);
			}
		}
		eed= eed->next;
	}

	index= 0;
	eve= em->verts.first;
	while(eve) {
		if(eve->f & SELECT) {
			if(eve->f1) {
				
				int xaxis= RNA_boolean_get(op->ptr, "xaxis");
				int yaxis= RNA_boolean_get(op->ptr, "yaxis");
				int zaxis= RNA_boolean_get(op->ptr, "zaxis");
				
				if (((Mesh *)obedit->data)->editflag & ME_EDIT_MIRROR_X) {
					eve_mir= editmesh_get_x_mirror_vert(obedit, em, eve, eve->co, index);
				}
				
				adr = eve->tmp.p;
				fac= 0.5/(float)eve->f1;
				
				if(xaxis)
					eve->co[0]= 0.5*eve->co[0]+fac*adr[0];
				if(yaxis)
					eve->co[1]= 0.5*eve->co[1]+fac*adr[1];
				if(zaxis)
					eve->co[2]= 0.5*eve->co[2]+fac*adr[2];
				
				
				/* clip if needed by mirror modifier */
				if (eve->f2) {
					if (eve->f2 & 1) {
						eve->co[0]= 0.0f;
					}
					if (eve->f2 & 2) {
						eve->co[1]= 0.0f;
					}
					if (eve->f2 & 4) {
						eve->co[2]= 0.0f;
					}
				}
				
				if (eve_mir) {
					eve_mir->co[0]=-eve->co[0];
					eve_mir->co[1]= eve->co[1];
					eve_mir->co[2]= eve->co[2];
				}
				
			}
			eve->tmp.p= NULL;
		}
		index++;
		eve= eve->next;
	}
	MEM_freeN(adror);

	recalc_editnormals(em);

	BKE_mesh_end_editmesh(obedit->data, em);

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

static int smooth_vertex_exec(bContext *C, wmOperator *op)
{
	int repeat = RNA_int_get(op->ptr, "repeat");
	int i;

	if (!repeat) repeat = 1;

	for (i=0; i<repeat; i++) {
		smooth_vertex(C, op);
	}

	return OPERATOR_FINISHED;
}

void MESH_OT_vertices_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Smooth Vertex";
	ot->description= "Flatten angles of selected vertices";
	ot->idname= "MESH_OT_vertices_smooth";
	
	/* api callbacks */
	ot->exec= smooth_vertex_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_int(ot->srna, "repeat", 1, 1, 100, "Smooth Iterations", "", 1, INT_MAX);
	RNA_def_boolean(ot->srna, "xaxis", 1, "X-Axis", "Smooth along the X axis.");
	RNA_def_boolean(ot->srna, "yaxis", 1, "Y-Axis", "Smooth along the Y axis.");
	RNA_def_boolean(ot->srna, "zaxis", 1, "Z-Axis", "Smooth along the Z axis.");
}

void vertexnoise(Object *obedit, EditMesh *em)
{
	Material *ma;
	Tex *tex;
	EditVert *eve;
	float b2, ofs, vec[3];

	if(em==NULL) return;
	
	ma= give_current_material(obedit, obedit->actcol);
	if(ma==0 || ma->mtex[0]==0 || ma->mtex[0]->tex==0) {
		return;
	}
	tex= ma->mtex[0]->tex;
	
	ofs= tex->turbul/200.0;
	
	eve= (struct EditVert *)em->verts.first;
	while(eve) {
		if(eve->f & SELECT) {
			
			if(tex->type==TEX_STUCCI) {
				
				b2= BLI_hnoise(tex->noisesize, eve->co[0], eve->co[1], eve->co[2]);
				if(tex->stype) ofs*=(b2*b2);
				vec[0]= 0.2*(b2-BLI_hnoise(tex->noisesize, eve->co[0]+ofs, eve->co[1], eve->co[2]));
				vec[1]= 0.2*(b2-BLI_hnoise(tex->noisesize, eve->co[0], eve->co[1]+ofs, eve->co[2]));
				vec[2]= 0.2*(b2-BLI_hnoise(tex->noisesize, eve->co[0], eve->co[1], eve->co[2]+ofs));
				
				add_v3_v3(eve->co, vec);
			}
			else {
				float tin, dum;
				externtex(ma->mtex[0], eve->co, &tin, &dum, &dum, &dum, &dum);
				eve->co[2]+= 0.05*tin;
			}
		}
		eve= eve->next;
	}

	recalc_editnormals(em);
//	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);

}

void flipface(EditMesh *em, EditFace *efa)
{
	if(efa->v4) {
		SWAP(EditVert *, efa->v2, efa->v4);
		SWAP(EditEdge *, efa->e1, efa->e4);
		SWAP(EditEdge *, efa->e2, efa->e3);
		EM_data_interp_from_faces(em, efa, NULL, efa, 0, 3, 2, 1);
	}
	else {
		SWAP(EditVert *, efa->v2, efa->v3);
		SWAP(EditEdge *, efa->e1, efa->e3);
		efa->e2->dir= 1-efa->e2->dir;
		EM_data_interp_from_faces(em, efa, NULL, efa, 0, 2, 1, 3);
	}

	if(efa->v4) normal_quad_v3( efa->n,efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co);
	else normal_tri_v3( efa->n,efa->v1->co, efa->v2->co, efa->v3->co);
}


static int flip_normals(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	EditFace *efa;
	
	efa= em->faces.first;
	while(efa) {
		if( efa->f & SELECT ){
			flipface(em, efa);
		}
		efa= efa->next;
	}
	
	/* update vertex normals too */
	recalc_editnormals(em);

	BKE_mesh_end_editmesh(obedit->data, em);

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}

void MESH_OT_flip_normals(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Flip Normals";
	ot->description= "Toggle the direction of selected face's vertex and face normals";
	ot->idname= "MESH_OT_flip_normals";
	
	/* api callbacks */
	ot->exec= flip_normals;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


static int solidify_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	float nor[3] = {0,0,1};

	float thickness= RNA_float_get(op->ptr, "thickness");

	extrudeflag(obedit, em, SELECT, nor, 1);
	EM_make_hq_normals(em);
	EM_solidify(em, thickness);


	/* update vertex normals too */
	recalc_editnormals(em);

	BKE_mesh_end_editmesh(obedit->data, em);

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}


void MESH_OT_solidify(wmOperatorType *ot)
{
	PropertyRNA *prop;
	/* identifiers */
	ot->name= "Solidify";
	ot->description= "Create a solid skin by extruding, compensating for sharp angles";
	ot->idname= "MESH_OT_solidify";

	/* api callbacks */
	ot->exec= solidify_exec;
	ot->poll= ED_operator_editmesh;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	prop= RNA_def_float(ot->srna, "thickness", 0.01f, -FLT_MAX, FLT_MAX, "Thickness", "", -10.0f, 10.0f);
	RNA_def_property_ui_range(prop, -10, 10, 0.1, 4);
}

static int mesh_select_nth_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	int nth = RNA_int_get(op->ptr, "nth");

	if(EM_deselect_nth(em, nth) == 0) {
		BKE_report(op->reports, RPT_ERROR, "Mesh has no active vert/edge/face.");
		return OPERATOR_CANCELLED;
	}

	BKE_mesh_end_editmesh(obedit->data, em);

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}


void MESH_OT_select_nth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Nth";
	ot->description= "";
	ot->idname= "MESH_OT_select_nth";

	/* api callbacks */
	ot->exec= mesh_select_nth_exec;
	ot->poll= ED_operator_editmesh;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_int(ot->srna, "nth", 2, 2, 100, "Nth Selection", "", 1, INT_MAX);
}

