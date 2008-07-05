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
 * The Original Code is Copyright (C) 2004 by NaN Holding BV.
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
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#ifdef WITH_VERSE
#include "BKE_verse.h"
#endif

#include "BIF_editmesh.h"
#include "BIF_resources.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_meshtools.h"
#include "BIF_mywindow.h"
#include "BIF_previewrender.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_editsima.h"

#ifdef WITH_VERSE
#include "BIF_verse.h"
#endif

#include "BDR_drawobject.h"
#include "BDR_editobject.h"
#include "BDR_editface.h"

#include "BSE_drawview.h"
#include "BSE_edit.h"
#include "BSE_view.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "RE_render_ext.h"  /* externtex */

#include "multires.h"
#include "mydevice.h"
#include "blendef.h"

#include "editmesh.h"


/* ****************************** MIRROR **************** */

void EM_select_mirrored(void)
{
	if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		EditMesh *em = G.editMesh;
		EditVert *eve, *v1;
		
		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->f & SELECT) {
				v1= editmesh_get_x_mirror_vert(G.obedit, eve->co);
				if(v1) {
					eve->f &= ~SELECT;
					v1->f |= SELECT;
				}
			}
		}
	}
}

void EM_automerge(int update) {
	int len;
	if ((G.scene->automerge) &&
		(G.obedit && G.obedit->type==OB_MESH) &&
		(((Mesh*)G.obedit->data)->mr==NULL)
	  ) {
		len = removedoublesflag(1, 1, G.scene->toolsettings->doublimit);
		if (len) {
			G.totvert -= len; /* saves doing a countall */
			if (update) {
				DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
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
int EM_init_backbuf_border(short xmin, short ymin, short xmax, short ymax)
{
	struct ImBuf *buf;
	unsigned int *dr;
	int a;
	
	if(G.obedit==NULL || G.vd->drawtype<OB_SOLID || (G.vd->flag & V3D_ZBUF_SELECT)==0) return 0;
	if(em_vertoffs==0) return 0;
	
	buf= read_backbuf(xmin, ymin, xmax, ymax);
	if(buf==NULL) return 0;

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
int EM_mask_init_backbuf_border(short mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax)
{
	unsigned int *dr, *drm;
	struct ImBuf *buf, *bufmask;
	int a;
	
	/* method in use for face selecting too */
	if(G.obedit==NULL) {
		if(FACESEL_PAINT_TEST);
		else return 0;
	}
	else if(G.vd->drawtype<OB_SOLID || (G.vd->flag & V3D_ZBUF_SELECT)==0) return 0;

	if(em_vertoffs==0) return 0;
	
	buf= read_backbuf(xmin, ymin, xmax, ymax);
	if(buf==NULL) return 0;

	dr = buf->rect;

	/* draw the mask */
#ifdef __APPLE__
	glDrawBuffer(GL_AUX0);
#endif
	glDisable(GL_DEPTH_TEST);
	
	persp(PERSP_WIN);
	glColor3ub(0, 0, 0);
	
	/* yah, opengl doesn't do concave... tsk! */
 	draw_triangulated(mcords, tot);	
	
	glBegin(GL_LINE_LOOP);	/* for zero sized masks, lines */
	for(a=0; a<tot; a++) glVertex2s(mcords[a][0], mcords[a][1]);
	glEnd();
	
	persp(PERSP_VIEW);
	glFinish();	/* to be sure readpixels sees mask */
	
	glDrawBuffer(GL_BACK);
	
	/* grab mask */
	bufmask= read_backbuf(xmin, ymin, xmax, ymax);
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
int EM_init_backbuf_circle(short xs, short ys, short rads)
{
	struct ImBuf *buf;
	unsigned int *dr;
	short xmin, ymin, xmax, ymax, xc, yc;
	int radsq;
	
	/* method in use for face selecting too */
	if(G.obedit==NULL) {
		if(FACESEL_PAINT_TEST);
		else return 0;
	}
	else if(G.vd->drawtype<OB_SOLID || (G.vd->flag & V3D_ZBUF_SELECT)==0) return 0;
	if(em_vertoffs==0) return 0;
	
	xmin= xs-rads; xmax= xs+rads;
	ymin= ys-rads; ymax= ys+rads;
	buf= read_backbuf(xmin, ymin, xmax, ymax);
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




static unsigned int findnearestvert__backbufIndextest(unsigned int index){
		EditVert *eve = BLI_findlink(&G.editMesh->verts, index-1);
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
EditVert *findnearestvert(int *dist, short sel, short strict)
{
	short mval[2];

	getmouseco_areawin(mval);
	if(G.vd->drawtype>OB_WIRE && (G.vd->flag & V3D_ZBUF_SELECT)){
		int distance;
		unsigned int index;
		EditVert *eve;
		
		if(strict) index = sample_backbuf_rect(mval, 50, em_wireoffs, 0xFFFFFF, &distance, strict, findnearestvert__backbufIndextest); 
		else index = sample_backbuf_rect(mval, 50, em_wireoffs, 0xFFFFFF, &distance, 0, NULL); 
		
		eve = BLI_findlink(&G.editMesh->verts, index-1);
		
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

		if (lastSelected && BLI_findlink(&G.editMesh->verts, lastSelectedIndex)!=lastSelected) {
			lastSelectedIndex = 0;
			lastSelected = NULL;
		}

		data.lastIndex = lastSelectedIndex;
		data.mval[0] = mval[0];
		data.mval[1] = mval[1];
		data.select = sel;
		data.dist = *dist;
		data.strict = strict;
		data.closest = NULL;
		data.closestIndex = 0;

		data.pass = 0;
		mesh_foreachScreenVert(findnearestvert__doClosest, &data, 1);

		if (data.dist>3) {
			data.pass = 1;
			mesh_foreachScreenVert(findnearestvert__doClosest, &data, 1);
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

/* note; uses G.vd, so needs active 3d window */
static void findnearestedge__doClosest(void *userData, EditEdge *eed, int x0, int y0, int x1, int y1, int index)
{
	struct { float mval[2]; int dist; EditEdge *closest; } *data = userData;
	float v1[2], v2[2];
	int distance;
		
	v1[0] = x0;
	v1[1] = y0;
	v2[0] = x1;
	v2[1] = y1;
		
	distance= PdistVL2Dfl(data->mval, v1, v2);
		
	if(eed->f & SELECT) distance+=5;
	if(distance < data->dist) {
		if(G.vd->flag & V3D_CLIPPING) {
			float labda= labda_PdistVL2Dfl(data->mval, v1, v2);
			float vec[3];

			vec[0]= eed->v1->co[0] + labda*(eed->v2->co[0] - eed->v1->co[0]);
			vec[1]= eed->v1->co[1] + labda*(eed->v2->co[1] - eed->v1->co[1]);
			vec[2]= eed->v1->co[2] + labda*(eed->v2->co[2] - eed->v1->co[2]);
			Mat4MulVecfl(G.obedit->obmat, vec);

			if(view3d_test_clipping(G.vd, vec)==0) {
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
EditEdge *findnearestedge(int *dist)
{
	short mval[2];
		
	getmouseco_areawin(mval);

	if(G.vd->drawtype>OB_WIRE && (G.vd->flag & V3D_ZBUF_SELECT)) {
		int distance;
		unsigned int index = sample_backbuf_rect(mval, 50, em_solidoffs, em_wireoffs, &distance,0, NULL);
		EditEdge *eed = BLI_findlink(&G.editMesh->edges, index-1);

		if (eed && distance<*dist) {
			*dist = distance;
			return eed;
		} else {
			return NULL;
		}
	}
	else {
		struct { float mval[2]; int dist; EditEdge *closest; } data;

		data.mval[0] = mval[0];
		data.mval[1] = mval[1];
		data.dist = *dist;
		data.closest = NULL;

		mesh_foreachScreenEdge(findnearestedge__doClosest, &data, 2);

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
static EditFace *findnearestface(int *dist)
{
	short mval[2];

	getmouseco_areawin(mval);

	if(G.vd->drawtype>OB_WIRE && (G.vd->flag & V3D_ZBUF_SELECT)) {
		unsigned int index = sample_backbuf(mval[0], mval[1]);
		EditFace *efa = BLI_findlink(&G.editMesh->faces, index-1);

		if (efa) {
			struct { short mval[2]; int dist; EditFace *toFace; } data;

			data.mval[0] = mval[0];
			data.mval[1] = mval[1];
			data.dist = 0x7FFF;		/* largest short */
			data.toFace = efa;

			mesh_foreachScreenFace(findnearestface__getDistance, &data);

			if(G.scene->selectmode == SCE_SELECT_FACE || data.dist<*dist) {	/* only faces, no dist check */
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

		if (lastSelected && BLI_findlink(&G.editMesh->faces, lastSelectedIndex)!=lastSelected) {
			lastSelectedIndex = 0;
			lastSelected = NULL;
		}

		data.lastIndex = lastSelectedIndex;
		data.mval[0] = mval[0];
		data.mval[1] = mval[1];
		data.dist = *dist;
		data.closest = NULL;
		data.closestIndex = 0;

		data.pass = 0;
		mesh_foreachScreenFace(findnearestface__doClosest, &data);

		if (data.dist>3) {
			data.pass = 1;
			mesh_foreachScreenFace(findnearestface__doClosest, &data);
		}

		*dist = data.dist;
		lastSelected = data.closest;
		lastSelectedIndex = data.closestIndex;

		return data.closest;
	}
}

/* for interactivity, frontbuffer draw in current window */
static void draw_dm_mapped_vert__mapFunc(void *theVert, int index, float *co, float *no_f, short *no_s)
{
	if (EM_get_vert_for_index(index)==theVert) {
		bglVertex3fv(co);
	}
}
static void draw_dm_mapped_vert(DerivedMesh *dm, EditVert *eve)
{
	EM_init_index_arrays(1, 0, 0);
	bglBegin(GL_POINTS);
	dm->foreachMappedVert(dm, draw_dm_mapped_vert__mapFunc, eve);
	bglEnd();
	EM_free_index_arrays();
}

static int draw_dm_mapped_edge__setDrawOptions(void *theEdge, int index)
{
	return EM_get_edge_for_index(index)==theEdge;
}
static void draw_dm_mapped_edge(DerivedMesh *dm, EditEdge *eed)
{
	EM_init_index_arrays(0, 1, 0);
	dm->drawMappedEdges(dm, draw_dm_mapped_edge__setDrawOptions, eed);
	EM_free_index_arrays();
}

static void draw_dm_mapped_face_center__mapFunc(void *theFace, int index, float *cent, float *no)
{
	if (EM_get_face_for_index(index)==theFace) {
		bglVertex3fv(cent);
	}
}
static void draw_dm_mapped_face_center(DerivedMesh *dm, EditFace *efa)
{
	EM_init_index_arrays(0, 0, 1);
	bglBegin(GL_POINTS);
	dm->foreachMappedFaceCenter(dm, draw_dm_mapped_face_center__mapFunc, efa);
	bglEnd();
	EM_free_index_arrays();
}

static void unified_select_draw(EditVert *eve, EditEdge *eed, EditFace *efa)
{
	DerivedMesh *dm = editmesh_get_derived_cage(CD_MASK_BAREMESH);

	glDrawBuffer(GL_FRONT);

	persp(PERSP_VIEW);
	
	if(G.vd->flag & V3D_CLIPPING)
		view3d_set_clipping(G.vd);
	
	glPushMatrix();
	mymultmatrix(G.obedit->obmat);
	
	/* face selected */
	if(efa) {
		if(G.scene->selectmode & SCE_SELECT_VERTEX) {
			glPointSize(BIF_GetThemeValuef(TH_VERTEX_SIZE));
			
			if(efa->f & SELECT) BIF_ThemeColor(TH_VERTEX_SELECT);
			else BIF_ThemeColor(TH_VERTEX);
			
			bglBegin(GL_POINTS);
			bglVertex3fv(efa->v1->co);
			bglVertex3fv(efa->v2->co);
			bglVertex3fv(efa->v3->co);
			if(efa->v4) bglVertex3fv(efa->v4->co);
			bglEnd();
		}

		if(G.scene->selectmode & (SCE_SELECT_EDGE|SCE_SELECT_FACE)) {
			if(efa->fgonf==0) {
				BIF_ThemeColor((efa->f & SELECT)?TH_EDGE_SELECT:TH_WIRE);
	
				draw_dm_mapped_edge(dm, efa->e1);
				draw_dm_mapped_edge(dm, efa->e2);
				draw_dm_mapped_edge(dm, efa->e3);
				if (efa->e4) {
					draw_dm_mapped_edge(dm, efa->e4);
				}
			}
		}
		
		if( CHECK_OB_DRAWFACEDOT(G.scene, G.vd, G.obedit->dt) ) {
			if(efa->fgonf==0) {
				glPointSize(BIF_GetThemeValuef(TH_FACEDOT_SIZE));
				BIF_ThemeColor((efa->f & SELECT)?TH_FACE_DOT:TH_WIRE);

				draw_dm_mapped_face_center(dm, efa);
			}
		}
	}
	/* edge selected */
	if(eed) {
		if(G.scene->selectmode & (SCE_SELECT_EDGE|SCE_SELECT_FACE)) {
			BIF_ThemeColor((eed->f & SELECT)?TH_EDGE_SELECT:TH_WIRE);

			draw_dm_mapped_edge(dm, eed);
		}
		if(G.scene->selectmode & SCE_SELECT_VERTEX) {
			glPointSize(BIF_GetThemeValuef(TH_VERTEX_SIZE));
			
			BIF_ThemeColor((eed->f & SELECT)?TH_VERTEX_SELECT:TH_VERTEX);
			
			draw_dm_mapped_vert(dm, eed->v1);
			draw_dm_mapped_vert(dm, eed->v2);
		}
	}
	if(eve) {
		if(G.scene->selectmode & SCE_SELECT_VERTEX) {
			glPointSize(BIF_GetThemeValuef(TH_VERTEX_SIZE));
			
			BIF_ThemeColor((eve->f & SELECT)?TH_VERTEX_SELECT:TH_VERTEX);
			
			draw_dm_mapped_vert(dm, eve);
		}
	}

	glPointSize(1.0);
	glPopMatrix();

	bglFlush();
	glDrawBuffer(GL_BACK);

	if(G.vd->flag & V3D_CLIPPING)
		view3d_clr_clipping();
	
	/* signal that frontbuf differs from back */
	curarea->win_swap= WIN_FRONT_OK;

	dm->release(dm);
}


/* best distance based on screen coords. 
   use g.scene->selectmode to define how to use 
   selected vertices and edges get disadvantage
   return 1 if found one
*/
static int unified_findnearest(EditVert **eve, EditEdge **eed, EditFace **efa) 
{
	int dist= 75;
	
	*eve= NULL;
	*eed= NULL;
	*efa= NULL;
	
	if(G.scene->selectmode & SCE_SELECT_VERTEX)
		*eve= findnearestvert(&dist, SELECT, 0);
	if(G.scene->selectmode & SCE_SELECT_FACE)
		*efa= findnearestface(&dist);

	dist-= 20;	/* since edges select lines, we give dots advantage of 20 pix */
	if(G.scene->selectmode & SCE_SELECT_EDGE)
		*eed= findnearestedge(&dist);

	/* return only one of 3 pointers, for frontbuffer redraws */
	if(*eed) {
		*efa= NULL; *eve= NULL;
	}
	else if(*efa) {
		*eve= NULL;
	}
	
	return (*eve || *eed || *efa);
}

/* this as a way to compare the ares, perim  of 2 faces thay will scale to different sizes
 *0.5 so smaller faces arnt ALWAYS selected with a thresh of 1.0 */
#define SCALE_CMP(a,b) ((a+a*thresh >= b) && (a-(a*thresh*0.5) <= b))

/* ****************  GROUP SELECTS ************** */
/* selects new faces/edges/verts based on the
 existing selection

FACES GROUP
 mode 1: same material
 mode 2: same image
 mode 3: same area
 mode 4: same perimeter
 mode 5: same normal
 mode 6: same co-planer
*/
int facegroup_select(short mode)
{
	EditMesh *em = G.editMesh;
	EditFace *efa, *base_efa=NULL;
	unsigned int selcount=0; /*count how many new faces we select*/
	
	/*deselcount, count how many deselected faces are left, so we can bail out early
	also means that if there are no deselected faces, we can avoid a lot of looping */
	unsigned int deselcount=0; 
	
	short ok=0;
	float thresh=G.scene->toolsettings->select_thresh;
	
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
	
	/*if mode is 3 then record face areas, 4 record perimeter */
	if (mode==3) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			efa->tmp.fp= EM_face_area(efa);
		}
	} else if (mode==4) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			efa->tmp.fp= EM_face_perimeter(efa);
		}
	}
	
	for(base_efa= em->faces.first; base_efa; base_efa= base_efa->next) {
		if (base_efa->f1) { /* This was one of the faces originaly selected */
			if (mode==1) { /* same material */
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
			} else if (mode==2) { /* same image */
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
			} else if (mode==3 || mode==4) { /* same area OR same perimeter, both use the same temp var */
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
			} else if (mode==5) { /* same normal */
				float angle;
				for(efa= em->faces.first; efa; efa= efa->next) {
					if (!(efa->f & SELECT) && !efa->h) {
						angle= VecAngle2(base_efa->n, efa->n);
						if (angle/180.0<=thresh) {
							EM_select_face(efa, 1);
							selcount++;
							deselcount--;
							if (!deselcount) /*have we selected all posible faces?, if so return*/
								return selcount;
						}
					}
				}
			} else if (mode==6) { /* same planer */
				float angle, base_dot, dot;
				base_dot= Inpf(base_efa->cent, base_efa->n);
				for(efa= em->faces.first; efa; efa= efa->next) {
					if (!(efa->f & SELECT) && !efa->h) {
						angle= VecAngle2(base_efa->n, efa->n);
						if (angle/180.0<=thresh) {
							dot=Inpf(efa->cent, base_efa->n);
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


/*
EDGE GROUP
 mode 1: same length
 mode 2: same direction
 mode 3: same number of face users
 mode 4: similar face angles.
 mode 5: similar crease
 mode 6: similar seam
 mode 7: similar sharp
*/

/* this function is only used by edgegroup_select's edge angle */



static int edgegroup_select__internal(short mode)
{
	EditMesh *em = G.editMesh;
	EditEdge *eed, *base_eed=NULL;
	unsigned int selcount=0; /* count how many new edges we select*/
	
	/*count how many visible selected edges there are,
	so we can return when there are none left */
	unsigned int deselcount=0;
	
	short ok=0;
	float thresh=G.scene->toolsettings->select_thresh;
	
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
			eed->f2=0; /* only for mode 4, edge animations */
		}
	}
	
	if (!ok || !deselcount) /* no data selected OR no more data to select*/
		return 0;
	
	if (mode==1) { /*store length*/
		for(eed= em->edges.first; eed; eed= eed->next) {
			if (!eed->h) /* dont calc data for hidden edges*/
				eed->tmp.fp= VecLenf(eed->v1->co, eed->v2->co);
		}
	} else if (mode==3) { /*store face users*/
		EditFace *efa;
		/* cound how many faces each edge uses use tmp->l */
		for(efa= em->faces.first; efa; efa= efa->next) {
			efa->e1->tmp.l++;
			efa->e2->tmp.l++;
			efa->e3->tmp.l++;
			if (efa->e4) efa->e4->tmp.l++;
		}
	} else if (mode==4) { /*store edge angles */
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
						eed->tmp.fp= VecAngle2(eed->tmp.f->n, efa->n)/180;
					eed->f2++; /* f2==0 no face assigned. f2==1 one face found. f2==2 angle calculated.*/
				}
				j++;
			}
		}
	}
	
	for(base_eed= em->edges.first; base_eed; base_eed= base_eed->next) {
		if (base_eed->f1) {
			if (mode==1) { /* same length */
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
			} else if (mode==2) { /* same direction */
				float base_dir[3], dir[3], angle;
				VecSubf(base_dir, base_eed->v1->co, base_eed->v2->co);
				for(eed= em->edges.first; eed; eed= eed->next) {
					if (!(eed->f & SELECT) && !eed->h) {
						VecSubf(dir, eed->v1->co, eed->v2->co);
						angle= VecAngle2(base_dir, dir);
						
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
			} else if (mode==3) { /* face users */				
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
			} else if (mode==4 && base_eed->f2==2) { /* edge angles, f2==2 means the edge has an angle. */				
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
			} else if (mode==5) { /* edge crease */
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
			} else if (mode==6) { /* edge seam */
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
			} else if (mode==7) { /* edge sharp */
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
int edgegroup_select(short mode)
{
	int selcount = edgegroup_select__internal(mode);
	
	if (selcount) {
		/* Could run a generic flush function,
		 * but the problem is only that all edges of a face
		 * can be selected without the face becoming selected */
		EditMesh *em = G.editMesh;
		EditFace *efa;
		for(efa= em->faces.first; efa; efa= efa->next) {
			if (efa->v4) {
				if (efa->e1->f&SELECT && efa->e2->f&SELECT && efa->e3->f&SELECT && efa->e4->f&SELECT)
					efa->f |= SELECT;
			}  else {
				if (efa->e1->f&SELECT && efa->e2->f&SELECT && efa->e3->f&SELECT)
					efa->f |= SELECT;
			}
		}
	}
	return selcount;
}



/*
VERT GROUP
 mode 1: same normal
 mode 2: same number of face users
 mode 3: same vertex groups
*/
int vertgroup_select(short mode)
{
	EditMesh *em = G.editMesh;
	EditVert *eve, *base_eve=NULL;
	
	unsigned int selcount=0; /* count how many new edges we select*/
	
	/*count how many visible selected edges there are,
	so we can return when there are none left */
	unsigned int deselcount=0;
	
	short ok=0;
	float thresh=G.scene->toolsettings->select_thresh;
	
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
	
	if (!ok || !deselcount) /* no data selected OR no more data to select*/
		return 0;
	
	
	if (mode==2) { /* store face users */
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
				
			if (mode==1) { /* same normal */
				float angle;
				for(eve= em->verts.first; eve; eve= eve->next) {
					if (!(eve->f & SELECT) && !eve->h) {
						angle= VecAngle2(base_eve->no, eve->no);
						if (angle/180.0<=thresh) {
							eve->f |= SELECT;
							selcount++;
							deselcount--;
							if (!deselcount) /*have we selected all posible faces?, if so return*/
								return selcount;
						}
					}
				}
			} else if (mode==2) { /* face users */
				for(eve= em->verts.first; eve; eve= eve->next) {
					if (
						!(eve->f & SELECT) &&
						!eve->h &&
						base_eve->tmp.l==eve->tmp.l
					) {
						eve->f |= SELECT;
						selcount++;
						deselcount--;
						if (!deselcount) /*have we selected all posible faces?, if so return*/
							return selcount;
					}
				}
			} else if (mode==3) { /* vertex groups */
				MDeformVert *dvert, *base_dvert;
				short i, j; /* weight index */

				base_dvert= CustomData_em_get(&em->vdata, base_eve->data,
					CD_MDEFORMVERT);

				if (!base_dvert || base_dvert->totweight == 0)
					return selcount;
				
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
									if (!deselcount) /*have we selected all posible faces?, if so return*/
										return selcount;
									break;
								}
							}
						}
					}
				}
			}
		}
	} /* end basevert loop */
	return selcount;
}

/* EditMode menu triggered from space.c by pressing Shift+G
handles face/edge vert context and
facegroup_select/edgegroup_select/vertgroup_select do all the work
*/

void select_mesh_group_menu()
{
	short ret;
	int selcount, first_item=1, multi=0;
	char str[512] = "Select Similar "; /* total max length is 404 at the moment */
	
	if (!ELEM3(G.scene->selectmode, SCE_SELECT_VERTEX, SCE_SELECT_EDGE, SCE_SELECT_FACE)) {
		multi=1;
	}
	
	if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		if (multi) strcat(str, "%t|Vertices%x-1|");
		else strcat(str, "Vertices %t|");
		strcat(str, "    Normal %x1|    Face Users %x2|    Shared Vertex Groups%x3");
		first_item=0;
	}

	if(G.scene->selectmode & SCE_SELECT_EDGE) {
		if (multi) {
			if (first_item) strcat(str, "%t|Edges%x-1|");
			else strcat(str, "|%l|Edges%x-1|");
		} else strcat(str, "Edges %t|");
		
		strcat(str, "    Length %x10|    Direction %x20|    Face Users%x30|    Face Angle%x40|    Crease%x50|    Seam%x60|    Sharp%x70");
		first_item=0;
	}
	
	if(G.scene->selectmode & SCE_SELECT_FACE) {
		if (multi) {
			strcat(str, "|%l|Faces%x-1|");
		} else strcat(str, "Faces %t|");
		strcat(str, "    Material %x100|    Image %x200|    Area %x300|    Perimeter %x400|    Normal %x500|    Co-Planar %x600");
	
	}
	
	ret= pupmenu(str);
	if (ret<1) return;
	
	if (ret<10) {
		selcount= vertgroup_select(ret);
		if (selcount) { /* update if data was selected */
			EM_select_flush(); /* so that selected verts, go onto select faces */
			G.totvertsel += selcount;
			allqueue(REDRAWVIEW3D, 0);
			if (EM_texFaceCheck())
				allqueue(REDRAWIMAGE, 0);
			BIF_undo_push("Select Similar Vertices");
		}
		return;
	}
	
	if (ret<100) {
		selcount= edgegroup_select(ret/10);
		
		if (selcount) { /* update if data was selected */
			/*EM_select_flush();*/ /* dont use because it can end up selecting more edges and is not usefull*/
			G.totedgesel+=selcount;
			allqueue(REDRAWVIEW3D, 0);
			if (EM_texFaceCheck())
				allqueue(REDRAWIMAGE, 0);
			BIF_undo_push("Select Similar Edges");
		}
		return;
	}
	
	if (ret<1000) {
		selcount= facegroup_select(ret/100);
		if (selcount) { /* update if data was selected */
			G.totfacesel+=selcount;
			allqueue(REDRAWVIEW3D, 0);
			if (EM_texFaceCheck())
				allqueue(REDRAWIMAGE, 0);
			BIF_undo_push("Select Similar Faces");
		}
		return;
	}
}

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
void mesh_layers_menu_concat(CustomData *data, int type, char *str) {
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

/* ctrl+c in mesh editmode */
void mesh_copy_menu(void)
{
	EditMesh *em = G.editMesh;
	EditSelection *ese;
	short ret, change=0;
	
	if (!em) return;
	
	ese = em->selected.last;
	
	/* Faces can have a NULL ese, so dont return on a NULL ese here */
	
	if(ese && ese->type == EDITVERT) {
		
		if (!ese) return;
		/*EditVert *ev, *ev_act = (EditVert*)ese->data;
		ret= pupmenu("");*/
	} else if(ese && ese->type == EDITEDGE) {
		EditEdge *eed, *eed_act;
		float vec[3], vec_mid[3], eed_len, eed_len_act;
		
		if (!ese) return;
		
		eed_act = (EditEdge*)ese->data;
		
		ret= pupmenu("Copy Active Edge to Selected%t|Crease%x1|Bevel Weight%x2|Length%x3");
		if (ret<1) return;
		
		eed_len_act = VecLenf(eed_act->v1->co, eed_act->v2->co);
		
		switch (ret) {
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
			
			for(eed=em->edges.first; eed; eed=eed->next) {
				if (eed->f & SELECT && eed != eed_act) {
					
					eed_len = VecLenf(eed->v1->co, eed->v2->co);
					
					if (eed_len == eed_len_act) continue;
					/* if this edge is zero length we cont do anything with it*/
					if (eed_len == 0.0f) continue;
					if (eed_len_act == 0.0f) {
						VecAddf(vec_mid, eed->v1->co, eed->v2->co);
						VecMulf(vec_mid, 0.5);
						VECCOPY(eed->v1->co, vec_mid);
						VECCOPY(eed->v2->co, vec_mid);
					} else {
						/* copy the edge length */
						VecAddf(vec_mid, eed->v1->co, eed->v2->co);
						VecMulf(vec_mid, 0.5);
						
						/* SCALE 1 */
						VecSubf(vec, eed->v1->co, vec_mid);
						VecMulf(vec, eed_len_act/eed_len);
						VecAddf(eed->v1->co, vec, vec_mid);
						
						/* SCALE 2 */
						VecSubf(vec, eed->v2->co, vec_mid);
						VecMulf(vec, eed_len_act/eed_len);
						VecAddf(eed->v2->co, vec, vec_mid);
					}
					change = 1;
				}
			}
			
			if (change)
				recalc_editnormals();
			
			
			break;
		}
		
	} else if(ese==NULL || ese->type == EDITFACE) {
		EditFace *efa, *efa_act;
		MTFace *tf, *tf_act = NULL;
		MCol *mcol, *mcol_act = NULL;
		
		efa_act = EM_get_actFace(0);
		
		if (efa_act) {
			ret= pupmenu(
				"Copy Face Selected%t|"
				"Active Material%x1|Active Image%x2|Active UV Coords%x3|"
				"Active Mode%x4|Active Transp%x5|Active Vertex Colors%x6|%l|"
				
				"TexFace UVs from layer%x7|"
				"TexFace Images from layer%x8|"
				"TexFace All from layer%x9|"
				"Vertex Colors from layer%x10");
			if (ret<1) return;
			tf_act =	CustomData_em_get(&em->fdata, efa_act->data, CD_MTFACE);
			mcol_act =	CustomData_em_get(&em->fdata, efa_act->data, CD_MCOL);
		} else {
			ret= pupmenu(
				"Copy Face Selected%t|"
				
				/* Make sure these are always the same as above */
				"TexFace UVs from layer%x7|"
				"TexFace Images from layer%x8|"
				"TexFace All from layer%x9|"
				"Vertex Colors from layer%x10");
			if (ret<1) return;
		}
		
		switch (ret) {
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
				error("mesh has no uv/image layers");
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
				error("mesh has no uv/image layers");
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
				error("mesh has no uv/image layers");
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
				error("mesh has no uv/image layers");
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
				error("mesh has no color layers");
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
		
		/* Copy from layer - Warning! tf_act and mcol_act will be NULL here */
		case 7:
		case 8:
		case 9:
			if (CustomData_number_of_layers(&em->fdata, CD_MTFACE)<2) {
				error("mesh does not have multiple uv/image layers");
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
				error("mesh does not have multiple color layers");
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
		switch (ret) {
		case 7: /* copy UV's only */
			for(efa=em->faces.first; efa; efa=efa->next) {
				if (efa->f & SELECT) {
					tf_act = (MTFace *)efa->tmp.p; /* not active but easier to use this way */
					tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					memcpy(tf->uv, tf_act->uv, sizeof(tf->uv));
					change = 1;
				}
			}
			break;
		case 8: /* copy image settings only */
			for(efa=em->faces.first; efa; efa=efa->next) {
				if (efa->f & SELECT) {
					tf_act = (MTFace *)efa->tmp.p; /* not active but easier to use this way */
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
		case 9: /* copy all tface info */
			for(efa=em->faces.first; efa; efa=efa->next) {
				if (efa->f & SELECT) {
					tf_act = (MTFace *)efa->tmp.p; /* not active but easier to use this way */
					tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					memcpy(tf->uv, ((MTFace *)efa->tmp.p)->uv, sizeof(tf->uv));
					tf->tpage = tf_act->tpage;
					tf->mode = tf_act->mode;
					tf->transp = tf_act->transp;
					change = 1;
				}
			}
			break;
		case 10:
			for(efa=em->faces.first; efa; efa=efa->next) {
				if (efa->f & SELECT) {
					mcol_act = (MCol *)efa->tmp.p; 
					mcol = CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
					memcpy(mcol, mcol_act, sizeof(MCol)*4);	
					change = 1;
				}
			}
			break;
		}
		
	}
	
	if (change) {
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		
		if (ese==NULL ||	ese->type == EDITFACE)	BIF_undo_push("Copy Face Attribute");
		else if (			ese->type == EDITEDGE)	BIF_undo_push("Copy Edge Attribute");
		else if (			ese->type == EDITVERT)	BIF_undo_push("Copy Vert Attribute");
		
	}
	
}


/* ****************  LOOP SELECTS *************** */

/* selects quads in loop direction of indicated edge */
/* only flush over edges with valence <= 2 */
void faceloop_select(EditEdge *startedge, int select)
{
	EditMesh *em = G.editMesh;
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
			if(efa->e4 && efa->f1==0) {	/* not done quad */
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
static int edge_not_in_tagged_face(EditEdge *eed)
{
	EditMesh *em = G.editMesh;
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
static void edgeloop_select(EditEdge *starteed, int select)
{
	EditMesh *em = G.editMesh;
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
					if(edge_not_in_tagged_face(eed)) {
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
static void edgering_select(EditEdge *startedge, int select){
	EditMesh *em = G.editMesh;
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

void loop_multiselect(int looptype)
{
	EditEdge *eed;
	EditEdge **edarray;
	int edindex, edfirstcount;
	
	/*edarray = MEM_mallocN(sizeof(*edarray)*G.totedgesel,"edge array");*/
	edarray = MEM_mallocN(sizeof(EditEdge*)*G.totedgesel,"edge array");
	edindex = 0;
	edfirstcount = G.totedgesel;
	
	for(eed=G.editMesh->edges.first; eed; eed=eed->next){
		if(eed->f&SELECT){
			edarray[edindex] = eed;
			edindex += 1;
		}
	}
	
	if(looptype){
		for(edindex = 0; edindex < edfirstcount; edindex +=1){
			eed = edarray[edindex];
			edgering_select(eed,SELECT);
		}
		countall();
		EM_selectmode_flush();
		BIF_undo_push("Edge Ring Multi-Select");
	}
	else{
		for(edindex = 0; edindex < edfirstcount; edindex +=1){
			eed = edarray[edindex];
			edgeloop_select(eed,SELECT);
		}
		countall();
		EM_selectmode_flush();
		BIF_undo_push("Edge Loop Multi-Select");
	}
	MEM_freeN(edarray);
	allqueue(REDRAWVIEW3D,0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
}
		
/* ***************** MAIN MOUSE SELECTION ************** */

/* just to have the functions nice together */

static void mouse_mesh_loop(void)
{
	EditEdge *eed;
	int select= 1;
	int dist= 50;
	
	eed= findnearestedge(&dist);
	if(eed) {
		if (G.scene->toolsettings->edge_mode == EDGE_MODE_SELECT) {
			if((G.qual & LR_SHIFTKEY)==0) EM_clear_flag_all(SELECT);
		
			if((eed->f & SELECT)==0) select=1;
			else if(G.qual & LR_SHIFTKEY) select=0;

			if(G.scene->selectmode & SCE_SELECT_FACE) {
				faceloop_select(eed, select);
			}
			else if(G.scene->selectmode & SCE_SELECT_EDGE) {
		        if(G.qual == (LR_CTRLKEY | LR_ALTKEY) || G.qual == (LR_CTRLKEY | LR_ALTKEY |LR_SHIFTKEY))
					edgering_select(eed, select);
		        else if(G.qual & LR_ALTKEY)
					edgeloop_select(eed, select);
			}
		    else if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		        if(G.qual == (LR_CTRLKEY | LR_ALTKEY) || G.qual == (LR_CTRLKEY | LR_ALTKEY |LR_SHIFTKEY))
					edgering_select(eed, select);
		        else if(G.qual & LR_ALTKEY)
					edgeloop_select(eed, select);
			}

			/* frontbuffer draw of last selected only */
			unified_select_draw(NULL, eed, NULL);
		
			EM_selectmode_flush();
			countall();
			allqueue(REDRAWVIEW3D, 0);
			if (EM_texFaceCheck())
				allqueue(REDRAWIMAGE, 0);
		} else { /*(G.scene->toolsettings->edge_mode == EDGE_MODE_TAG_*)*/
			int act = (edgetag_context_check(eed)==0);
			int path = 0;
			
			if (G.qual == (LR_SHIFTKEY | LR_ALTKEY) && G.editMesh->selected.last) {
				EditSelection *ese = G.editMesh->selected.last;
	
				if(ese && ese->type == EDITEDGE) {
					EditEdge *eed_act;
					eed_act = (EditEdge*)ese->data;
					if (eed_act != eed) {
						/* If shift is pressed we need to use the last active edge, (if it exists) */
						if (edgetag_shortest_path(eed_act, eed)) {
							EM_remove_selection(eed_act, EDITEDGE);
							EM_select_edge(eed_act, 0);
							path = 1;
						}
					}
				}
			}
			if (path==0) {
				edgetag_context_set(eed, act); /* switch the edge option */
			}
			
			if (act) {
				if ((eed->f & SELECT)==0) {
					EM_select_edge(eed, 1);
					EM_selectmode_flush();
					countall();
				}
				/* even if this is selected it may not be in the selection list */
				EM_store_selection(eed, EDITEDGE);
			} else {
				if (eed->f & SELECT) {
					EM_select_edge(eed, 0);
					/* logic is differnt from above here since if this was selected we dont know if its in the selection list or not */
					EM_remove_selection(eed, EDITEDGE);
					
					EM_selectmode_flush();
					countall();
				}
			}
			
			switch (G.scene->toolsettings->edge_mode) {
			case EDGE_MODE_TAG_SEAM:
				G.f |= G_DRAWSEAMS;
				break;
			case EDGE_MODE_TAG_SHARP:
				G.f |= G_DRAWSHARP;
				break;
			case EDGE_MODE_TAG_CREASE:	
				G.f |= G_DRAWCREASES;
				break;
			case EDGE_MODE_TAG_BEVEL:
				G.f |= G_DRAWBWEIGHTS;
				break;
			}
			
			unified_select_draw(NULL, eed, NULL);
			
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
		}
				
	}
}


/* here actual select happens */
void mouse_mesh(void)
{
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	
	if(G.qual & LR_ALTKEY) mouse_mesh_loop();
	else if(unified_findnearest(&eve, &eed, &efa)) {
		
		if((G.qual & LR_SHIFTKEY)==0) EM_clear_flag_all(SELECT);
		
		if(efa) {
			/* set the last selected face */
			EM_set_actFace(efa);
			
			if( (efa->f & SELECT)==0 ) {
				EM_store_selection(efa, EDITFACE);
				EM_select_face_fgon(efa, 1);
			}
			else if(G.qual & LR_SHIFTKEY) {
				EM_remove_selection(efa, EDITFACE);
				EM_select_face_fgon(efa, 0);
			}
		}
		else if(eed) {
			if((eed->f & SELECT)==0) {
				EM_store_selection(eed, EDITEDGE);
				EM_select_edge(eed, 1);
			}
			else if(G.qual & LR_SHIFTKEY) {
				EM_remove_selection(eed, EDITEDGE);
				EM_select_edge(eed, 0);
			}
		}
		else if(eve) {
			if((eve->f & SELECT)==0) {
				eve->f |= SELECT;
				EM_store_selection(eve, EDITVERT);
			}
			else if(G.qual & LR_SHIFTKEY){ 
				EM_remove_selection(eve, EDITVERT);
				eve->f &= ~SELECT;
			}
		}
		
		/* frontbuffer draw of last selected only */
		unified_select_draw(eve, eed, efa);
	
		EM_selectmode_flush();
		countall();
		  
		allqueue(REDRAWVIEW3D, 0);
		if (EM_texFaceCheck()) {
			allqueue(REDRAWIMAGE, 0);
			allqueue(REDRAWBUTSEDIT, 0); /* for the texture face panel */
		}
		if (efa && efa->mat_nr != G.obedit->actcol-1) {
			G.obedit->actcol= efa->mat_nr+1;
			allqueue(REDRAWBUTSEDIT, 0);
			allqueue(REDRAWBUTSSHADING, 0);
			BIF_preview_changed(ID_MA);
		}
	}

	rightmouse_transform();
}


void selectconnected_mesh_all(void)
{
	EditMesh *em = G.editMesh;
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
	EM_select_flush();
	
	countall();

	allqueue(REDRAWVIEW3D, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
	BIF_undo_push("Select Connected (All)");
}

void selectconnected_mesh(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve, *v1, *v2;
	EditEdge *eed;
	EditFace *efa;
	short done=1, sel, toggle=0;

	if(em->edges.first==0) return;
	
	if( unified_findnearest(&eve, &eed, &efa)==0 ) {
		/* error("Nothing indicated "); */ /* this is mostly annoying, eps with occluded geometry */
		return;
	}
	
	sel= 1;
	if(G.qual & LR_SHIFTKEY) sel=0;

	/* clear test flags */
	for(v1= em->verts.first; v1; v1= v1->next) v1->f1= 0;
	
	/* start vertex/face/edge */
	if(eve) eve->f1= 1;
	else if(eed) eed->v1->f1= eed->v2->f1= 1;
	else efa->v1->f1= efa->v2->f1= efa->v3->f1= 1;
	
	/* set flag f1 if affected */
	while(done==1) {
		done= 0;
		toggle++;
		
		if(toggle & 1) eed= em->edges.first;
		else eed= em->edges.last;
		
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
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->v1->f1 && eed->v2->f1) 
			EM_select_edge(eed, sel);
	}
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->v1->f1 && efa->v2->f1 && efa->v3->f1 && (efa->v4==NULL || efa->v4->f1)) 
			EM_select_face(efa, sel);
	}
	/* no flush needed, connected geometry is done */
	
	countall();
	
	allqueue(REDRAWVIEW3D, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
	
	BIF_undo_push("Select Linked");
	
}

/* for use with selectconnected_delimit_mesh only! */
#define is_edge_delimit_ok(eed) ((eed->tmp.l == 1) && (eed->seam==0))
#define is_face_tag(efa) is_edge_delimit_ok(efa->e1) || is_edge_delimit_ok(efa->e2) || is_edge_delimit_ok(efa->e3) || (efa->v4 && is_edge_delimit_ok(efa->e4))

#define face_tag(efa)\
	if(efa->v4)	efa->tmp.l=		efa->e1->tmp.l= efa->e2->tmp.l= efa->e3->tmp.l= efa->e4->tmp.l= 1;\
	else		efa->tmp.l=		efa->e1->tmp.l= efa->e2->tmp.l= efa->e3->tmp.l= 1;

/* all - 1) use all faces for extending the selection  2) only use the mouse face
 * sel - 1) select  0) deselect 
 * */
static void selectconnected_delimit_mesh__internal(short all, short sel)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	short done=1, change=0;
	int dist = 75;
	EditEdge *eed;
	if(em->faces.first==0) return;
	
	/* flag all edges as off*/
	for(eed= em->edges.first; eed; eed= eed->next)
		eed->tmp.l=0;
	
	if (all) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if (efa->f & SELECT) {
				face_tag(efa);
			} else {
				efa->tmp.l = 0;
			}
		}
	} else {
		EditFace *efa_mouse = findnearestface(&dist);
		
		if( !efa_mouse ) {
			/* error("Nothing indicated "); */ /* this is mostly annoying, eps with occluded geometry */
			return;
		}
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			efa->tmp.l = 0;
		}
		efa_mouse->tmp.l = 1;
		face_tag(efa_mouse);
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
		return;
	
	if (!sel) /* make sure de-selecting faces didnt de-select the verts/edges connected to selected faces, this is common with boundries */
		for(efa= em->faces.first; efa; efa= efa->next)
			if (efa->f & SELECT)
				EM_select_face(efa, 1);
	
	countall();
	
	allqueue(REDRAWVIEW3D, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
	
	BIF_undo_push("Select Linked Delimeted");
	
}

#undef is_edge_delimit_ok
#undef is_face_tag
#undef face_tag

void selectconnected_delimit_mesh(void)
{
	selectconnected_delimit_mesh__internal(0, ((G.qual & LR_SHIFTKEY)==0));
}
void selectconnected_delimit_mesh_all(void)
{
	selectconnected_delimit_mesh__internal(1, 1);
}	
	
	
/* swap is 0 or 1, if 1 it hides not selected */
void hide_mesh(int swap)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	int a;
	
	if(G.obedit==0) return;

	/* hide happens on least dominant select mode, and flushes up, not down! (helps preventing errors in subsurf) */
	/*  - vertex hidden, always means edge is hidden too
		- edge hidden, always means face is hidden too
		- face hidden, only set face hide
		- then only flush back down what's absolute hidden
	*/
	if(G.scene->selectmode & SCE_SELECT_VERTEX) {
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
	else if(G.scene->selectmode & SCE_SELECT_EDGE) {

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
	
	if(G.scene->selectmode & SCE_SELECT_FACE) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->h) a= 1; else a= 2;
			efa->e1->f1 |= a;
			efa->e2->f1 |= a;
			efa->e3->f1 |= a;
			if(efa->e4) efa->e4->f1 |= a;
			/* When edges are not delt with in their own loop, we need to explicitly re-selct select edges that are joined to unselected faces */
			if (swap && (G.scene->selectmode == SCE_SELECT_FACE) && (efa->f & SELECT)) {
				EM_select_face(efa, 1);
			}
		}
	}
	
	if(G.scene->selectmode >= SCE_SELECT_EDGE) {
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->f1==1) eed->h |= 1;
			if(eed->h & 1) a= 1; else a= 2;
			eed->v1->f1 |= a;
			eed->v2->f1 |= a;
		}
	}

	if(G.scene->selectmode >= SCE_SELECT_VERTEX) {
		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->f1==1) eve->h= 1;
		}
	}
	
	G.totedgesel= G.totfacesel= G.totvertsel= 0;
	allqueue(REDRAWVIEW3D, 0);
	if(EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	
	BIF_undo_push("Hide");
}


void reveal_mesh(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	
	if(G.obedit==0) return;

	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->h) {
			eve->h= 0;
			eve->f |= SELECT;
		}
	}
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->h & 1) {
			eed->h &= ~1;
			if(G.scene->selectmode & SCE_SELECT_VERTEX); 
			else EM_select_edge(eed, 1);
		}
	}
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->h) {
			efa->h= 0;
			if(G.scene->selectmode & (SCE_SELECT_EDGE|SCE_SELECT_VERTEX)); 
			else EM_select_face(efa, 1);
		}
	}

	EM_fgon_flags();	/* redo flags and indices for fgons */
	EM_selectmode_flush();
	countall();

	allqueue(REDRAWVIEW3D, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	
	BIF_undo_push("Reveal");
}

void hide_tface_uv(int swap)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	MTFace *tface;
	
	if( is_uv_tface_editing_allowed()==0 ) return;

	/* call the mesh function if we are in mesh sync sel */
	if (G.sima->flag & SI_SYNC_UVSEL) {
		hide_mesh(swap);
		return;
	}
	
	if(swap) {
		for (efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if (G.sima->flag & SI_SELACTFACE) {
					/* Pretend face mode */
					if ((	(efa->v4==NULL && 
							(	tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3)) ==			(TF_SEL1|TF_SEL2|TF_SEL3) )			 ||
							(	tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)) ==	(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)	) == 0) {
						
						if (G.scene->selectmode == SCE_SELECT_FACE) {
							efa->f &= ~SELECT;
							/* must re-select after */
							efa->e1->f &= ~SELECT;
							efa->e2->f &= ~SELECT;
							efa->e3->f &= ~SELECT;
							if(efa->e4) efa->e4->f &= ~SELECT;
						} else {
							EM_select_face(efa, 0);
						}
					}
					tface->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				} else if (G.scene->selectmode == SCE_SELECT_FACE) {
					if((tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3))==0) {
						if(!efa->v4)
							EM_select_face(efa, 0);
						else if(!(tface->flag & TF_SEL4))
							EM_select_face(efa, 0);
						tface->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
					}
				} else {
					/* EM_deselect_flush will deselect the face */
					if((tface->flag & TF_SEL1)==0)				efa->v1->f &= ~SELECT;
					if((tface->flag & TF_SEL2)==0)				efa->v2->f &= ~SELECT;
					if((tface->flag & TF_SEL3)==0)				efa->v3->f &= ~SELECT;
					if((efa->v4) && (tface->flag & TF_SEL4)==0)	efa->v4->f &= ~SELECT;			
					tface->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				}
			}
		}
	} else {
		for (efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if (G.sima->flag & SI_SELACTFACE) {
					if (	(efa->v4==NULL && 
							(	tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3)) ==			(TF_SEL1|TF_SEL2|TF_SEL3) )			 ||
							(	tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)) ==	(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4)	) {
						
						if (G.scene->selectmode == SCE_SELECT_FACE) {
							efa->f &= ~SELECT;
							/* must re-select after */
							efa->e1->f &= ~SELECT;
							efa->e2->f &= ~SELECT;
							efa->e3->f &= ~SELECT;
							if(efa->e4) efa->e4->f &= ~SELECT;
						} else {
							EM_select_face(efa, 0);
						}
					}
					tface->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				} else if (G.scene->selectmode == SCE_SELECT_FACE) {
					if(tface->flag & (TF_SEL1|TF_SEL2|TF_SEL3))
						EM_select_face(efa, 0);
					else if(efa->v4 && tface->flag & TF_SEL4)
						EM_select_face(efa, 0);
					tface->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				} else {
					/* EM_deselect_flush will deselect the face */
					if(tface->flag & TF_SEL1)				efa->v1->f &= ~SELECT;
					if(tface->flag & TF_SEL2)				efa->v2->f &= ~SELECT;
					if(tface->flag & TF_SEL3)				efa->v3->f &= ~SELECT;
					if((efa->v4) && tface->flag & TF_SEL4)	efa->v4->f &= ~SELECT;
					tface->flag &= ~(TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4);
				}
			}
		}
	}
	
	
	/*deselects too many but ok for now*/
	if(G.scene->selectmode & (SCE_SELECT_EDGE|SCE_SELECT_VERTEX)) {
		EM_deselect_flush();
	}
	
	if (G.scene->selectmode==SCE_SELECT_FACE) {
		/* de-selected all edges from faces that were de-selected.
		 * now make sure all faces that are selected also have selected edges */
		for (efa= em->faces.first; efa; efa= efa->next) {
			if (efa->f & SELECT) {
				EM_select_face(efa, 1);
			}
		}
	}
	
	EM_validate_selections();
	
	BIF_undo_push("Hide UV");

	object_tface_flags_changed(OBACT, 0);
}

void reveal_tface_uv(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	MTFace *tface;

	if( is_uv_tface_editing_allowed()==0 ) return;
	
	/* call the mesh function if we are in mesh sync sel */
	if (G.sima->flag & SI_SYNC_UVSEL) {
		reveal_mesh();
		return;
	}
	
	if (G.sima->flag & SI_SELACTFACE) {
		if (G.scene->selectmode == SCE_SELECT_FACE) {
			for (efa= em->faces.first; efa; efa= efa->next) {
				if (!(efa->h) && !(efa->f & SELECT)) {
					tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
					EM_select_face(efa, 1);
					tface->flag |= TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4;
				}
			}
		} else {
			/* enable adjacent faces to have disconnected UV selections if sticky is disabled */
			if (G.sima->sticky == SI_STICKY_DISABLE) {
				for (efa= em->faces.first; efa; efa= efa->next) {
					if (!(efa->h) && !(efa->f & SELECT)) {
						/* All verts must be unselected for the face to be selected in the UV view */
						if ((efa->v1->f&SELECT)==0 && (efa->v2->f&SELECT)==0 && (efa->v3->f&SELECT)==0 && (efa->v4==0 || (efa->v4->f&SELECT)==0)) {
							tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
							tface->flag |= TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4;
							/* Cant use EM_select_face here because it unselects the verts
							 * and we cant tell if the face was totally unselected or not */
							/*EM_select_face(efa, 1);
							 * 
							 * See Loop with EM_select_face() below... */
							efa->f |= SELECT;
						}
					}
				}
			} else {
				for (efa= em->faces.first; efa; efa= efa->next) {
					if (!(efa->h) && !(efa->f & SELECT)) {
						tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
						if ((efa->v1->f & SELECT)==0)				{tface->flag |= TF_SEL1;}
						if ((efa->v2->f & SELECT)==0)				{tface->flag |= TF_SEL2;}
						if ((efa->v3->f & SELECT)==0)				{tface->flag |= TF_SEL3;}
						if ((efa->v4 && (efa->v4->f & SELECT)==0))	{tface->flag |= TF_SEL4;}
						efa->f |= SELECT;
					}
				}
			}
			
			/* Select all edges and verts now */
			for (efa= em->faces.first; efa; efa= efa->next) {
				/* we only selected the face flags, and didnt changes edges or verts, fix this now */
				if (!(efa->h) && (efa->f & SELECT)) {
					EM_select_face(efa, 1);
				}
			}
			EM_select_flush();
		}
	} else if (G.scene->selectmode == SCE_SELECT_FACE) {
		for (efa= em->faces.first; efa; efa= efa->next) {
			if (!(efa->h) && !(efa->f & SELECT)) {
				tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				efa->f |= SELECT;
				tface->flag |= TF_SEL1|TF_SEL2|TF_SEL3|TF_SEL4;
			}
		}
		
		/* Select all edges and verts now */
		for (efa= em->faces.first; efa; efa= efa->next) {
			/* we only selected the face flags, and didnt changes edges or verts, fix this now */
			if (!(efa->h) && (efa->f & SELECT)) {
				EM_select_face(efa, 1);
			}
		}
		
	} else {
		for (efa= em->faces.first; efa; efa= efa->next) {
			if (!(efa->h) && !(efa->f & SELECT)) {
				tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
				if ((efa->v1->f & SELECT)==0)				{tface->flag |= TF_SEL1;}
				if ((efa->v2->f & SELECT)==0)				{tface->flag |= TF_SEL2;}
				if ((efa->v3->f & SELECT)==0)				{tface->flag |= TF_SEL3;}
				if ((efa->v4 && (efa->v4->f & SELECT)==0))	{tface->flag |= TF_SEL4;}
				efa->f |= SELECT;
			}
		}
		
		/* Select all edges and verts now */
		for (efa= em->faces.first; efa; efa= efa->next) {
			/* we only selected the face flags, and didnt changes edges or verts, fix this now */
			if (!(efa->h) && (efa->f & SELECT)) {
				EM_select_face(efa, 1);
			}
		}
	}
	
	BIF_undo_push("Reveal UV");
	
	object_tface_flags_changed(OBACT, 0);
}

void select_faces_by_numverts(int numverts)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;

	/* Selects trias/qiads or isolated verts, and edges that do not have 2 neighboring
	 * faces
	 */

	/* for loose vertices/edges, we first select all, loop below will deselect */
	if(numverts==5)
		EM_set_flag_all(SELECT);
	else if(G.scene->selectmode!=SCE_SELECT_FACE) {
		error("Only works in face selection mode");
		return;
	}
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		if (efa->e4) {
			EM_select_face(efa, (numverts==4) );
		}
		else {
			EM_select_face(efa, (numverts==3) );
		}
	}

	countall();
	addqueue(curarea->win,  REDRAW, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
	
	if (numverts==3)
		BIF_undo_push("Select Triangles");
	else if (numverts==4)
		BIF_undo_push("Select Quads");
	else
		BIF_undo_push("Select non-Triangles/Quads");
}

void select_sharp_edges(void)
{
	/* Find edges that have exactly two neighboring faces,
	 * check the angle between those faces, and if angle is
	 * small enough, select the edge
	 */
	EditMesh *em = G.editMesh;
	EditEdge *eed;
	EditFace *efa;
	EditFace **efa1;
	EditFace **efa2;
	long edgecount = 0, i;
	static short sharpness = 135;
	float fsharpness;

	if(G.scene->selectmode==SCE_SELECT_FACE) {
		error("Doesn't work in face selection mode");
		return;
	}

	if(button(&sharpness,0, 180,"Max Angle:")==0) return;
	/* if faces are at angle 'sharpness', then the face normals
	 * are at angle 180.0 - 'sharpness' (convert to radians too)
	 */
	fsharpness = ((180.0 - sharpness) * M_PI) / 180.0;

	i=0;
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

	countall();
	addqueue(curarea->win,  REDRAW, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
	
	BIF_undo_push("Select Sharp Edges");
}

void select_linked_flat_faces(void)
{
	/* Find faces that are linked to selected faces that are 
	 * relatively flat (angle between faces is higher than
	 * specified angle)
	 */
	EditMesh *em = G.editMesh;
	EditEdge *eed;
	EditFace *efa;
	EditFace **efa1;
	EditFace **efa2;
	long edgecount = 0, i, faceselcount=0, faceselcountold=0;
	static short sharpness = 135;
	float fsharpness;

	if(G.scene->selectmode!=SCE_SELECT_FACE) {
		error("Only works in face selection mode");
		return;
	}

	if(button(&sharpness,0, 180,"Min Angle:")==0) return;
	/* if faces are at angle 'sharpness', then the face normals
	 * are at angle 180.0 - 'sharpness' (convert to radians too)
	 */
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

	countall();
	addqueue(curarea->win,  REDRAW, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
	BIF_undo_push("Select Linked Flat Faces");
}

void select_non_manifold(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;

	/* Selects isolated verts, and edges that do not have 2 neighboring
	 * faces
	 */
	
	if(G.scene->selectmode==SCE_SELECT_FACE) {
		error("Doesn't work in face selection mode");
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
	if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		eve= em->verts.first;
		while(eve) {
			if (eve->f1 == 0) {
				if (!eve->h) eve->f |= SELECT;
			}
			eve= eve->next;
		}
	}

	countall();
	addqueue(curarea->win,  REDRAW, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
	BIF_undo_push("Select Non Manifold");
}

void selectswap_mesh(void) /* UI level */
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	
	if(G.scene->selectmode & SCE_SELECT_VERTEX) {

		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->h==0) {
				if(eve->f & SELECT) eve->f &= ~SELECT;
				else eve->f|= SELECT;
			}
		}
	}
	else if(G.scene->selectmode & SCE_SELECT_EDGE) {
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

	EM_selectmode_flush();
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);

	BIF_undo_push("Select Swap");
	
}

void deselectall_mesh(void)	 /* this toggles!!!, UI level */
{
	
	if(G.obedit->lay & G.vd->lay) {

		if( EM_nvertices_selected() ) {
			EM_clear_flag_all(SELECT);
			BIF_undo_push("Deselect All");
		}
		else  {
			EM_set_flag_all(SELECT);
			BIF_undo_push("Select All");
		}
		
		countall();
		
		if (EM_texFaceCheck())
			allqueue(REDRAWIMAGE, 0);
		
		allqueue(REDRAWVIEW3D, 0);
	}
}

void EM_select_more(void)
{
	EditMesh *em = G.editMesh;
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
	if(G.scene->selectmode <= SCE_SELECT_EDGE) {
		
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

void select_more(void)
{
	EM_select_more();

	countall();
	addqueue(curarea->win,  REDRAW, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
	BIF_undo_push("Select More");
}

void EM_select_less(void)
{
	EditMesh *em = G.editMesh;
	EditEdge *eed;
	EditFace *efa;

	if(G.scene->selectmode <= SCE_SELECT_EDGE) {
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
		EM_deselect_flush();
		
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
		EM_selectmode_flush();
		
	}
}

void select_less(void)
{
	EM_select_less();
	
	countall();
	BIF_undo_push("Select Less");
	allqueue(REDRAWVIEW3D, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
}


void selectrandom_mesh(void) /* randomly selects a user-set % of vertices/edges/faces */
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	static short randfac = 50;

	if(G.obedit==NULL || (G.obedit->lay & G.vd->lay)==0) return;

	/* Get the percentage of vertices to randomly select as 'randfac' */
	if(button(&randfac,0, 100,"Percentage:")==0) return;

	BLI_srand( BLI_rand() ); /* random seed */
	
	if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->h==0) {
				if ( (BLI_frand() * 100) < randfac) 
					eve->f |= SELECT;
			}
		}
		EM_selectmode_flush();
		countall();
		BIF_undo_push("Select Random: Vertices");
	}
	else if(G.scene->selectmode & SCE_SELECT_EDGE) {
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->h==0) {
				if ( (BLI_frand() * 100) < randfac) 
					EM_select_edge(eed, 1);
			}
		}
		EM_selectmode_flush();
		countall();
		BIF_undo_push("Select Random:Edges");
	}
	else {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->h==0) {
				if ( (BLI_frand() * 100) < randfac) 
					EM_select_face(efa, 1);
			}
		}
		
		EM_selectmode_flush();
		countall();
		BIF_undo_push("Select Random:Faces");
	}
	allqueue(REDRAWVIEW3D, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
}

void editmesh_select_by_material(int index) 
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	
	for (efa=em->faces.first; efa; efa= efa->next) {
		if (efa->mat_nr==index) {
			EM_select_face(efa, 1);
		}
	}

	EM_selectmode_flush();
}

void editmesh_deselect_by_material(int index) 
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	
	for (efa=em->faces.first; efa; efa= efa->next) {
		if (efa->mat_nr==index) {
			EM_select_face(efa, 0);
		}
	}

	EM_selectmode_flush();
}

void EM_selectmode_menu(void)
{
	int val;
	
	if(G.scene->selectmode & SCE_SELECT_VERTEX) pupmenu_set_active(1);
	else if(G.scene->selectmode & SCE_SELECT_EDGE) pupmenu_set_active(2);
	else pupmenu_set_active(3);
	
	val= pupmenu("Select Mode%t|Vertices|Edges|Faces");
	
	
	if(val>0) {
		if(val==1){ 
			G.scene->selectmode= SCE_SELECT_VERTEX;
			EM_selectmode_set();
			countall(); 
			BIF_undo_push("Selectmode Set: Vertex");
			}
		else if(val==2){
			if((G.qual==LR_CTRLKEY)) EM_convertsel(G.scene->selectmode, SCE_SELECT_EDGE);
			G.scene->selectmode= SCE_SELECT_EDGE;
			EM_selectmode_set();
			countall();
			BIF_undo_push("Selectmode Set: Edge");
		}
		
		else{
			if((G.qual==LR_CTRLKEY)) EM_convertsel(G.scene->selectmode, SCE_SELECT_FACE);
			G.scene->selectmode= SCE_SELECT_FACE;
			EM_selectmode_set();
			countall();
			BIF_undo_push("Selectmode Set: Vertex");
		}
		
		allqueue(REDRAWVIEW3D, 1);
		if (EM_texFaceCheck())
			allqueue(REDRAWIMAGE, 0);
	}
}

/* ************************* SEAMS AND EDGES **************** */

void editmesh_mark_seam(int clear)
{
	EditMesh *em= G.editMesh;
	EditEdge *eed;
	
	if(multires_level1_test()) return;

	/* auto-enable seams drawing */
	if(clear==0) {
		if(!(G.f & G_DRAWSEAMS)) {
			G.f |= G_DRAWSEAMS;
			allqueue(REDRAWBUTSEDIT, 0);
		}
	}

	if(clear) {
		eed= em->edges.first;
		while(eed) {
			if((eed->h==0) && (eed->f & SELECT)) {
				eed->seam = 0;
			}
			eed= eed->next;
		}
		BIF_undo_push("Mark Seam");
	}
	else {
		eed= em->edges.first;
		while(eed) {
			if((eed->h==0) && (eed->f & SELECT)) {
				eed->seam = 1;
			}
			eed= eed->next;
		}
		BIF_undo_push("Clear Seam");
	}

	allqueue(REDRAWVIEW3D, 0);
}

void editmesh_mark_sharp(int set)
{
	EditMesh *em= G.editMesh;
	EditEdge *eed;

#if 0
	/* auto-enable sharp edge drawing */
	if(set) {
		if(!(G.f & G_DRAWSEAMS)) {
			G.f |= G_DRAWSEAMS;
			allqueue(REDRAWBUTSEDIT, 0);
		}
	}
#endif

	if(multires_level1_test()) return;

	if(set) {
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

	allqueue(REDRAWVIEW3D, 0);
}

void BME_Menu()	{
	short ret;
	ret= pupmenu("BME modeller%t|Select Edges of Vert%x1");
	
	switch(ret)
	{
		case 1:
		//BME_edges_of_vert();
		break;
	}
}



void Vertex_Menu() {
	short ret;
	ret= pupmenu("Vertex Specials%t|Remove Doubles%x1|Merge%x2|Smooth %x3|Select Vertex Path%x4|Blend From Shape%x5|Propagate To All Shapes%x6");

	switch(ret)
	{
		case 1:
			notice("Removed %d Vertices", removedoublesflag(1, 0, G.scene->toolsettings->doublimit));
			BIF_undo_push("Remove Doubles");
			break;
		case 2:	
			mergemenu();
			break;
		case 3:
			vertexsmooth();
			break;
		case 4:
			pathselect();
			BIF_undo_push("Select Vertex Path");
			break;
		case 5: 
			shape_copy_select_from();
			break;
		case 6: 
			shape_propagate();
			break;
	}
	/* some items crashed because this is in the original W menu but not here. should really manage this better */
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
}


void Edge_Menu() {
	short ret;

	ret= pupmenu("Edge Specials%t|Mark Seam %x1|Clear Seam %x2|Rotate Edge CW%x3|Rotate Edge CCW%x4|Loopcut%x6|Edge Slide%x5|Edge Loop Select%x7|Edge Ring Select%x8|Loop to Region%x9|Region to Loop%x10|Mark Sharp%x11|Clear Sharp%x12");

	switch(ret)
	{
	case 1:
		editmesh_mark_seam(0);
		break;
	case 2:
		editmesh_mark_seam(1);
		break;
	case 3:
		edge_rotate_selected(2);
		break;
	case 4:
		edge_rotate_selected(1);
		break;
	case 5:
		EdgeSlide(0,0.0);
    		BIF_undo_push("EdgeSlide");
		break;
	case 6:
        	CutEdgeloop(1);
		BIF_undo_push("Loopcut New");
		break;
	case 7:
		loop_multiselect(0);
		break;
	case 8:
		loop_multiselect(1);
		break;
	case 9:
		loop_to_region();
		break;
	case 10:
		region_to_loop();
		break;
	case 11:
		editmesh_mark_sharp(1);
		BIF_undo_push("Mark Sharp");
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		break;
	case 12: 
		editmesh_mark_sharp(0);
		BIF_undo_push("Clear Sharp");
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		break;
	}
	/* some items crashed because this is in the original W menu but not here. should really manage this better */
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
}

void Face_Menu() {
	short ret;
	ret= pupmenu(
		"Face Specials%t|Flip Normals%x1|Bevel%x2|Shade Smooth%x3|Shade Flat%x4|"
		"Triangulate (Ctrl T)%x5|Quads from Triangles (Alt J)%x6|Flip Triangle Edges (Ctrl Shift F)%x7|%l|"
		"Face Mode Set%x8|Face Mode Clear%x9|%l|"
		"UV Rotate (Shift - CCW)%x10|UV Mirror (Shift - Switch Axis)%x11|"
		"Color Rotate (Shift - CCW)%x12|Color Mirror (Shift - Switch Axis)%x13");

	switch(ret)
	{
		case 1:
			flip_editnormals();
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
			BIF_undo_push("Flip Normals");
			allqueue(REDRAWVIEW3D, 0);
			break;
		case 2:
			bevel_menu();
			break;
		case 3:
			mesh_set_smooth_faces(1);
			break;
		case 4:
			mesh_set_smooth_faces(0);
			break;
			
		case 5: /* Quads to Tris */
			convert_to_triface(0);
			allqueue(REDRAWVIEW3D, 0);
			countall();
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
			break;
		case 6: /* Tris to Quads */
			join_triangles();
			break;
		case 7: /* Flip triangle edges */
			edge_flip();
			break;
		case 8:
			mesh_set_face_flags(1);
			break;
		case 9:
			mesh_set_face_flags(0);
			break;
			
		/* uv texface options */
		case 10:
			mesh_rotate_uvs();
			break;
		case 11:
			mesh_mirror_uvs();
			break;
		case 12:
			mesh_rotate_colors();
			break;
		case 13:
			mesh_mirror_colors();
			break;
	}
	/* some items crashed because this is in the original W menu but not here. should really manage this better */
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
}


/* **************** NORMALS ************** */

void righthandfaces(int select)	/* makes faces righthand turning */
{
	EditMesh *em = G.editMesh;
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
				CalcCent3f(cent, efa->v1->co, efa->v2->co, efa->v3->co);
				cent[0]= cent[0]*cent[0] + cent[1]*cent[1] + cent[2]*cent[2];
				
				if(cent[0]>maxx) {
					maxx= cent[0];
					startvl= efa;
					tria_nr= 0;
				}
				if(efa->v4) {
					CalcCent3f(cent, efa->v1->co, efa->v3->co, efa->v4->co);
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
			CalcNormFloat(startvl->v1->co, startvl->v3->co, startvl->v4->co, nor);
			CalcCent3f(cent, startvl->v1->co, startvl->v3->co, startvl->v4->co);
		} else {
			CalcNormFloat(startvl->v1->co, startvl->v2->co, startvl->v3->co, nor);
			CalcCent3f(cent, startvl->v1->co, startvl->v2->co, startvl->v3->co);
		}
		/* first normal is oriented this way or the other */
		if(select) {
			if(select==2) {
				if(cent[0]*nor[0]+cent[1]*nor[1]+cent[2]*nor[2] > 0.0) flipface(startvl);
			}
			else {
				if(cent[0]*nor[0]+cent[1]*nor[1]+cent[2]*nor[2] < 0.0) flipface(startvl);
			}
		}
		else if(cent[0]*nor[0]+cent[1]*nor[1]+cent[2]*nor[2] < 0.0) flipface(startvl);


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

							flipface(efa);

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

	recalc_editnormals();
	
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
#endif
	
	waitcursor(0);
}


/* ********** ALIGN WITH VIEW **************** */


static void editmesh_calc_selvert_center(float cent_r[3])
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	int nsel= 0;

	cent_r[0]= cent_r[1]= cent_r[0]= 0.0;

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
void faceselect_align_view_to_selected(View3D *v3d, Mesh *me, int axis)
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
				CalcNormFloat4(v1, v2, v3, v4, fno);
			} else {
				CalcNormFloat(v1, v2, v3, fno);
			}

			norm[0]+= fno[0];
			norm[1]+= fno[1];
			norm[2]+= fno[2];

			totselected++;
		}
	}

	if (totselected == 0)
		error("No faces selected.");
	else
		view3d_align_axis_to_vector(v3d, axis, norm);
}

/* helper for below, to survive non-uniform scaled objects */
static void face_getnormal_obspace(EditFace *efa, float *fno)
{
	float vec[4][3];
	
	VECCOPY(vec[0], efa->v1->co);
	Mat4Mul3Vecfl(G.obedit->obmat, vec[0]);
	VECCOPY(vec[1], efa->v2->co);
	Mat4Mul3Vecfl(G.obedit->obmat, vec[1]);
	VECCOPY(vec[2], efa->v3->co);
	Mat4Mul3Vecfl(G.obedit->obmat, vec[2]);
	if(efa->v4) {
		VECCOPY(vec[3], efa->v4->co);
		Mat4Mul3Vecfl(G.obedit->obmat, vec[3]);
		
		CalcNormFloat4(vec[0], vec[1], vec[2], vec[3], fno);
	}
	else CalcNormFloat(vec[0], vec[1], vec[2], fno);
}


void editmesh_align_view_to_selected(View3D *v3d, int axis)
{
	EditMesh *em = G.editMesh;
	int nselverts= EM_nvertices_selected();
	float norm[3]={0.0, 0.0, 0.0}; /* used for storing the mesh normal */
	
	if (nselverts==0) {
		error("No faces or vertices selected.");
	} 
	else if (EM_nfaces_selected()) {
		EditFace *efa;
		for (efa= em->faces.first; efa; efa= efa->next) {
			if (faceselectedAND(efa, SELECT)) {
				float fno[3];
				
				face_getnormal_obspace(efa, fno);
				norm[0]+= fno[0];
				norm[1]+= fno[1];
				norm[2]+= fno[2];
			}
		}

		view3d_align_axis_to_vector(v3d, axis, norm);
	} 
	else if (nselverts>2) {
		float cent[3];
		EditVert *eve, *leve= NULL;

		editmesh_calc_selvert_center(cent);
		for (eve= em->verts.first; eve; eve= eve->next) {
			if (eve->f & SELECT) {
				if (leve) {
					float tno[3];
					CalcNormFloat(cent, leve->co, eve->co, tno);
					
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

		Mat4Mul3Vecfl(G.obedit->obmat, norm);
		view3d_align_axis_to_vector(v3d, axis, norm);
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
		Mat4Mul3Vecfl(G.obedit->obmat, norm);
		view3d_align_axis_to_vector(v3d, axis, norm);
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
		Mat4Mul3Vecfl(G.obedit->obmat, norm);
		view3d_align_axis_to_vector(v3d, axis, norm);
	}
} 

/* **************** VERTEX DEFORMS *************** */

void vertexsmooth(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	float *adror, *adr, fac;
	float fvec[3];
	int teller=0;
	ModifierData *md= G.obedit->modifiers.first;

	if(G.obedit==0) return;

	/* count */
	eve= em->verts.first;
	while(eve) {
		if(eve->f & SELECT) teller++;
		eve= eve->next;
	}
	if(teller==0) return;
	
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
	for (; md; md=md->next) {
		if (md->type==eModifierType_Mirror) {
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
				VecAddf(eed->v1->tmp.p, eed->v1->tmp.p, fvec);
			}
			if((eed->v2->f & SELECT) && eed->v2->f1<255) {
				eed->v2->f1++;
				VecAddf(eed->v2->tmp.p, eed->v2->tmp.p, fvec);
			}
		}
		eed= eed->next;
	}

	eve= em->verts.first;
	while(eve) {
		if(eve->f & SELECT) {
			if(eve->f1) {
				adr = eve->tmp.p;
				fac= 0.5/(float)eve->f1;
				
				eve->co[0]= 0.5*eve->co[0]+fac*adr[0];
				eve->co[1]= 0.5*eve->co[1]+fac*adr[1];
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
			}
			eve->tmp.p= NULL;
		}
		eve= eve->next;
	}
	MEM_freeN(adror);

	recalc_editnormals();

	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_verseverts_with_editverts(G.editMesh->vnode);
#endif
	BIF_undo_push("Vertex Smooth");
}

void vertexnoise(void)
{
	EditMesh *em = G.editMesh;
	Material *ma;
	Tex *tex;
	EditVert *eve;
	float b2, ofs, vec[3];

	if(G.obedit==0) return;
	
	ma= give_current_material(G.obedit, G.obedit->actcol);
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
				
				VecAddf(eve->co, eve->co, vec);
			}
			else {
				float tin, dum;
				externtex(ma->mtex[0], eve->co, &tin, &dum, &dum, &dum, &dum);
				eve->co[2]+= 0.05*tin;
			}
		}
		eve= eve->next;
	}

	recalc_editnormals();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_verseverts_with_editverts(G.editMesh->vnode);
#endif
	BIF_undo_push("Vertex Noise");
}

void vertices_to_sphere(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	Object *ob= OBACT;
	float *curs, len, vec[3], cent[3], fac, facm, imat[3][3], bmat[3][3];
	int tot;
	short perc=100;
	
	if(ob==0) return;
	TEST_EDITMESH
	
	if(button(&perc, 1, 100, "Percentage:")==0) return;
	
	fac= perc/100.0;
	facm= 1.0-fac;
	
	Mat3CpyMat4(bmat, ob->obmat);
	Mat3Inv(imat, bmat);

	/* center */
	curs= give_cursor();
	cent[0]= curs[0]-ob->obmat[3][0];
	cent[1]= curs[1]-ob->obmat[3][1];
	cent[2]= curs[2]-ob->obmat[3][2];
	Mat3MulVecfl(imat, cent);

	len= 0.0;
	tot= 0;
	eve= em->verts.first;
	while(eve) {
		if(eve->f & SELECT) {
			tot++;
			len+= VecLenf(cent, eve->co);
		}
		eve= eve->next;
	}
	len/=tot;
	
	if(len==0.0) len= 10.0;
	
	eve= em->verts.first;
	while(eve) {
		if(eve->f & SELECT) {
			vec[0]= eve->co[0]-cent[0];
			vec[1]= eve->co[1]-cent[1];
			vec[2]= eve->co[2]-cent[2];
			
			Normalize(vec);
			
			eve->co[0]= fac*(cent[0]+vec[0]*len) + facm*eve->co[0];
			eve->co[1]= fac*(cent[1]+vec[1]*len) + facm*eve->co[1];
			eve->co[2]= fac*(cent[2]+vec[2]*len) + facm*eve->co[2];
			
		}
		eve= eve->next;
	}
	
	recalc_editnormals();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_verseverts_with_editverts(G.editMesh->vnode);
#endif
	BIF_undo_push("To Sphere");
}

