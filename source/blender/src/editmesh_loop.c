/**
 * $Id: 
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
 * The Original Code is Copyright (C) 2004 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/*

editmesh_loop: tools with own drawing subloops, select, knife, subdiv

*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"


#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_ghash.h"

#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#ifdef WITH_VERSE
#include "BKE_verse.h"
#endif

#include "BIF_cursors.h"
#include "BIF_editmesh.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#ifdef WITH_VERSE
#include "BIF_verse.h"
#endif

#include "BSE_view.h"
#include "BSE_edit.h"
#include "BSE_drawview.h"

#include "BDR_drawobject.h"
#include "BDR_editobject.h"
#include "PIL_time.h"

#include "mydevice.h"
#include "blendef.h"

#include "editmesh.h"

/* next 2 includes for knife tool... shouldnt be! (ton) */
#include "GHOST_C-api.h"
#include "winlay.h"


/* New LoopCut */
static void edgering_sel(EditEdge *startedge, int select, int previewlines)
{
	EditMesh *em = G.editMesh;
	EditEdge *eed;
	EditFace *efa;
	EditVert *v[2][2];
	float co[2][3];
	int looking= 1,i;
	
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
	
	// tag startedge OK
	startedge->f2= 1;
	
	while(looking) {
		looking= 0;
		
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->e4 && efa->f1==0 && efa->h == 0) {	// not done quad
				if(efa->e1->f1<=2 && efa->e2->f1<=2 && efa->e3->f1<=2 && efa->e4->f1<=2) { // valence ok

					// if edge tagged, select opposing edge and mark face ok
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
	
	if(previewlines > 0 && select == 0){
			persp(PERSP_VIEW);
			glPushMatrix();
			mymultmatrix(G.obedit->obmat);
			//glColor3ub(0, 255, 255);
			//glBegin(GL_LINES);			
			//glVertex3f(nearest->v1->co[0],nearest->v1->co[1],nearest->v1->co[2]);
			//glVertex3f(nearest->v2->co[0],nearest->v2->co[1],nearest->v2->co[2]);
			//glEnd();
			for(efa= em->faces.first; efa; efa= efa->next) {
				if(efa->v4 == NULL) {  continue; }
				if(efa->h == 0){
					if(efa->e1->f2 == 1){
						if(efa->e1->h == 1 || efa->e3->h == 1 )
							continue;
						
						v[0][0] = efa->v1;
						v[0][1] = efa->v2;
						v[1][0] = efa->v4;
						v[1][1] = efa->v3;
					} else if(efa->e2->f2 == 1){
						if(efa->e2->h == 1 || efa->e4->h == 1)
							continue;
						v[0][0] = efa->v2;
						v[0][1] = efa->v3;
						v[1][0] = efa->v1;
						v[1][1] = efa->v4;					
					} else { continue; }
										  
					for(i=1;i<=previewlines;i++){
						co[0][0] = (v[0][1]->co[0] - v[0][0]->co[0])*(i/((float)previewlines+1))+v[0][0]->co[0];
						co[0][1] = (v[0][1]->co[1] - v[0][0]->co[1])*(i/((float)previewlines+1))+v[0][0]->co[1];
						co[0][2] = (v[0][1]->co[2] - v[0][0]->co[2])*(i/((float)previewlines+1))+v[0][0]->co[2];

						co[1][0] = (v[1][1]->co[0] - v[1][0]->co[0])*(i/((float)previewlines+1))+v[1][0]->co[0];
						co[1][1] = (v[1][1]->co[1] - v[1][0]->co[1])*(i/((float)previewlines+1))+v[1][0]->co[1];
						co[1][2] = (v[1][1]->co[2] - v[1][0]->co[2])*(i/((float)previewlines+1))+v[1][0]->co[2];					
						glColor3ub(255, 0, 255);
						glBegin(GL_LINES);	
						glVertex3f(co[0][0],co[0][1],co[0][2]);
						glVertex3f(co[1][0],co[1][1],co[1][2]);
						glEnd();
					}
				}
			}
			glPopMatrix();   
	} else {	
	
	/* (de)select the edges */
		for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->f2) EM_select_edge(eed, select);
		}
	}
}
void CutEdgeloop(int numcuts)
{
	EditMesh *em = G.editMesh;
	EditEdge *nearest=NULL, *eed;
	float fac;
	int keys = 0, holdnum=0, selectmode, dist;
	short mvalo[2] = {0,0}, mval[2];
	short event, val, choosing=1, cancel=0, cuthalf = 0, smooth=0;
	short hasHidden = 0;
	char msg[128];
	
	selectmode = G.scene->selectmode;
		
	if(G.scene->selectmode & SCE_SELECT_FACE){
		G.scene->selectmode =  SCE_SELECT_EDGE;
		EM_selectmode_set();	  
	}
	
	
	BIF_undo_push("Loopcut Begin");
	while(choosing && !cancel){
		getmouseco_areawin(mval);
		if (mval[0] != mvalo[0] || mval[1] != mvalo[1]) {
			mvalo[0] = mval[0];
			mvalo[1] = mval[1];
			dist= 50;
			nearest = findnearestedge(&dist);	// returns actual distance in dist
			scrarea_do_windraw(curarea);	// after findnearestedge, backbuf!
			
			sprintf(msg,"Number of Cuts: %d",numcuts);
			if(smooth){
				sprintf(msg,"%s (S)mooth: on",msg);
			} else {
				sprintf(msg,"%s (S)mooth: off",msg);
			}
			
			headerprint(msg);
			/* Need to figure preview */
			if(nearest){
				edgering_sel(nearest, 0, numcuts);
			 }   
			screen_swapbuffers();
			
		/* backbuffer refresh for non-apples (no aux) */
#ifndef __APPLE__
			if(G.vd->drawtype>OB_WIRE && (G.vd->flag & V3D_ZBUF_SELECT)) {
				backdrawview3d(0);
			}
#endif
		}
		else PIL_sleep_ms(10);	// idle		
		
		
		while(qtest()) 
		{
			val=0;
			event= extern_qread(&val);
			if(val && (event ==  MOUSEX || event == MOUSEY)){ ; } 
			else if(val && ((event==LEFTMOUSE || event==RETKEY) || (event == MIDDLEMOUSE || event==PADENTER)))
			{
				if(event == MIDDLEMOUSE){
					cuthalf = 1;
				}
				if (nearest==NULL)
					cancel = 1;
				choosing=0;
				mvalo[0] = -1;
			}
			else if(val && (event==ESCKEY || event==RIGHTMOUSE ))
			{
				choosing=0;
				cancel = 1;
				mvalo[0] = -1;
			}
			else if(val && (event==PADPLUSKEY || event==WHEELUPMOUSE))
			{
				numcuts++;
				mvalo[0] = -1;
			}
			else if(val && (event==PADMINUS || event==WHEELDOWNMOUSE))
			{
				if(numcuts > 1){
					numcuts--;
					mvalo[0] = -1;
				} 
			}
			else if(val && event==SKEY)
			{
				if(smooth){smooth=0;} 
				else { smooth=1; }
				mvalo[0] = -1;
			}
			
			else if(val){
				holdnum = -1;
				switch(event){
					case PAD9:
					case NINEKEY:
						holdnum = 9; break;
					case PAD8:
					case EIGHTKEY:
						holdnum = 8;break;
					case PAD7:
					case SEVENKEY:
						holdnum = 7;break;
					case PAD6:
					case SIXKEY:
						holdnum = 6;break;
					case PAD5:
					case FIVEKEY:
						holdnum = 5;break;
					case PAD4:
					case FOURKEY:
						holdnum = 4;break;
					case PAD3:
					case THREEKEY:
						holdnum = 3; break;
					case PAD2:
					case TWOKEY:
						holdnum = 2;break;
					case PAD1:
					case ONEKEY:
						holdnum = 1; break;
					case PAD0:
					case ZEROKEY:
						holdnum = 0;break;	
					case BACKSPACEKEY:
						holdnum = -2;break;			
				}
				if(holdnum >= 0 && numcuts*10 < 130){
					if(keys == 0){  // first level numeric entry
							if(holdnum > 0){
									numcuts = holdnum;
									keys++;		
							}
					} else if(keys > 0){//highrt level numeric entry
							numcuts *= 10;
							numcuts += holdnum;
							keys++;		
					}
				} else if (holdnum == -2){// backspace
					if (keys > 1){
						numcuts /= 10;		
						keys--;
					} else {
						numcuts=1;
						keys = 0;
					}
				}
				mvalo[0] = -1;
				break;
			}  // End Numeric Entry						
		}  //End while(qtest())
	}   // End Choosing
	scrarea_queue_winredraw(curarea);

	if(cancel){
		return;   
	}
	/* clean selection */
	for(eed=em->edges.first; eed; eed = eed->next){
		EM_select_edge(eed,0);
	}
	/* select edge ring */
	edgering_sel(nearest, 1, 0);
	
	/* now cut the loops */
	if(smooth){
		fac= 1.0f;
		if(fbutton(&fac, 0.0f, 5.0f, 10, 10, "Smooth:")==0) return;
		fac= 0.292f*fac;			
		esubdivideflag(SELECT,fac,B_SMOOTH,numcuts,SUBDIV_SELECT_LOOPCUT);
	} else {
		esubdivideflag(SELECT,0,0,numcuts,SUBDIV_SELECT_LOOPCUT);
	}
	/* if this was a single cut, enter edgeslide mode */
	if(numcuts == 1 && hasHidden == 0){
		if(cuthalf)
			EdgeSlide(1,0.0);
		else {
			if(EdgeSlide(0,0.0) == -1){
				BIF_undo();
			}
		}
	}
	
	if(G.scene->selectmode !=  selectmode){
		G.scene->selectmode = selectmode;
		EM_selectmode_set();
	}	
	
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_verseverts_with_editverts((VNode*)G.editMesh->vnode);
#endif
	scrarea_queue_headredraw(curarea);
	scrarea_queue_winredraw(curarea);
	return;
}


/* *************** LOOP SELECT ************* */
#if 0
static short edgeFaces(EditEdge *e)
{
	EditMesh *em = G.editMesh;
	EditFace *search=NULL;
	short count = 0;
	
	search = em->faces.first;
	while(search){
		if((search->e1 == e || search->e2 == e) || (search->e3 == e || search->e4 == e)) 
			count++;
		search = search->next;
	}
	return count;	
}
#endif

/* this utility function checks to see if 2 edit edges share a face,
	returns 1 if they do
	returns 0 if they do not, or if the function is passed the same edge 2 times
*/
short sharesFace(EditEdge* e1, EditEdge* e2)
{
	EditMesh *em = G.editMesh;
	EditFace *search=NULL;
	
	search = em->faces.first;
	if (e1 == e2){
		return 0 ;
	}
	while(search){
		if(
			((search->e1 == e1 || search->e2 == e1) || (search->e3 == e1 || search->e4 == e1)) &&
			((search->e1 == e2 || search->e2 == e2) || (search->e3 == e2 || search->e4 == e2))
			) {
			return 1;
		}
		search = search->next;
	}
	return 0;
}


/*   ***************** TRAIL ************************

Read a trail of mouse coords and return them as an array of CutCurve structs
len returns number of mouse coords read before commiting with RETKEY   
It is up to the caller to free the block when done with it,

XXX Is only used here, so local inside this file (ton)
 */

#define TRAIL_POLYLINE 1 /* For future use, They don't do anything yet */
#define TRAIL_FREEHAND 2
#define TRAIL_MIXED    3 /* (1|2) */
#define TRAIL_AUTO     4 
#define	TRAIL_MIDPOINTS 8

typedef struct CutCurve {
	float  x; 
	float  y;
} CutCurve;

static CutCurve *get_mouse_trail(int *len, char mode, char cutmode, struct GHash *gh)
{
	CutCurve *curve,*temp;
	EditVert *snapvert;
	float *scr, mval[2]={0.0,0.0}, lastx=0, lasty=0, lockx=0, locky=0;
	int i=0, j, blocks=1, lasti=0;
	int dist, tolerance;
	short event, val, qual, vsnap=0, ldown=0, restart=0, rubberband=0;
	short mval1[2], lockaxis=0, oldmode; 
	
	*len=0;
	tolerance = 75;
	
	curve=(CutCurve *)MEM_callocN(1024*sizeof(CutCurve), "MouseTrail");

	if (!curve) {
		printf("failed to allocate memory in get_mouse_trail()\n");
		return(NULL);
	}
	mywinset(curarea->win);
	
	
	if(cutmode != KNIFE_MULTICUT){ 
		/*redraw backbuffer if in zbuffered selection mode but not vertex selection*/
		if(G.vd->drawtype>OB_WIRE && (G.vd->flag & V3D_ZBUF_SELECT)) {
			oldmode = G.scene->selectmode;
			G.scene->selectmode = SCE_SELECT_VERTEX;
			backdrawview3d(0);
			G.scene->selectmode = oldmode;
		}
		glDrawBuffer(GL_FRONT);
		headerprint("(LMB) draw, (Ctrl held while drawing) snap to vertex, (MMB) constrain to x/y screen axis, (Enter) cut "
					"(with Ctrl to select cut line), (Esc) cancel");
	}
	else{
		glDrawBuffer(GL_FRONT);
		headerprint("(LMB) draw, (MMB) constrain to x/y screen axis, (Enter) cut (with Ctrl to select cut line), (Esc) cancel");
	}
	
	persp(PERSP_WIN);
	
	glColor3ub(255, 0, 255);
	
	while(TRUE) {
		
		event=extern_qread(&val);	/* Enter or RMB indicates finish */
		if(val) {
			if(event==RETKEY || event==PADENTER) break;
		}
		
		if( event==ESCKEY || event==RIGHTMOUSE ) {
			if (curve) MEM_freeN(curve);
			*len=0;
			bglFlush();
			glDrawBuffer(GL_BACK);
			return(NULL);
			break;
		}	
		
		if (rubberband)  { /* rubberband mode, undraw last rubberband */
			glLineWidth(2.0);
			sdrawXORline((int)curve[i-1].x, (int)curve[i-1].y,(int)mval[0],(int) mval[1]); 
			glLineWidth(1.0);
			glFlush();
			rubberband=0;
		}
		
		/*handle vsnap*/
		vsnap = 0;
		if(cutmode != KNIFE_MULTICUT){
			qual = get_qual();
			if(qual & LR_CTRLKEY) vsnap = 1;
		}
		
		if(vsnap){ 
			persp(PERSP_VIEW);
			dist = tolerance;
			snapvert = findnearestvert(&dist, SELECT, 0);
			glColor3ub(255, 0, 255);
			glDrawBuffer(GL_FRONT);
			persp(PERSP_WIN);
			if(snapvert && (dist < tolerance)){
				scr = BLI_ghash_lookup(gh, snapvert);
				mval[0] = scr[0];
				mval[1] = scr[1];
			}
			else{ 
				getmouseco_areawin(mval1);
				mval[0] = (float)mval1[0];
				mval[1] = (float)mval1[1];
			}
		}
		
		else{
			getmouseco_areawin(mval1);
			mval[0] = (float)mval1[0];
			mval[1] = (float)mval1[1];
		}
		
		
		if (lockaxis==1) mval[1]=locky;
		if (lockaxis==2) mval[0]=lockx;
		
		if ( ((i==0) || (mval[0]!=curve[i-1].x) || (mval[1]!=curve[i-1].y))
			&& (get_mbut() & L_MOUSE) ){ /* record changes only, if LMB down */
			
			lastx=curve[i].x=mval[0];
			lasty=curve[i].y=mval[1];
			
			lockaxis=0;
			
			i++; 
			
			ldown=1;
			if (restart) { 
				for(j=1;j<i;j++) sdrawXORline((int)curve[j-1].x, (int)curve[j-1].y, (int)curve[j].x, (int)curve[j].y);
				if (rubberband) sdrawXORline((int)curve[j].x, (int)curve[j].y, (int)mval[0], (int)mval[1]);
				glFlush();
				rubberband=0;
				lasti=i=0;
				restart=0;
				ldown=0;
			}
		}
		
		if ((event==MIDDLEMOUSE)&&(get_mbut()&M_MOUSE)&&(i)){/*MMB Down*/
		/*determine which axis to lock to, or clear if locked */
			if (lockaxis) lockaxis=0;
			else if (abs(curve[i-1].x-mval[0]) > abs(curve[i-1].y-mval[1])) lockaxis=1;
			else lockaxis=2;
			
			if (lockaxis) {
				lockx=lastx;
				locky=lasty;
			}
		}
		
		if ((i>1)&&(i!=lasti)) {  /*Draw recorded part of curve */
			sdrawline((int)curve[i-2].x, (int)curve[i-2].y, (int)curve[i-1].x, (int)curve[i-1].y);
			glFlush();
		}
		
		if ((i==lasti)&&(i>0)) { /*Draw rubberband */
			glLineWidth(2.0);
			sdrawXORline((int)curve[i-1].x, (int)curve[i-1].y,(int)mval[0], (int)mval[1]);
			glLineWidth(1.0);
			glFlush();
			rubberband=1;
		}
		lasti=i;

		if (i>=blocks*1024) { /* reallocate data if out of room */
			temp=curve;
			curve=(CutCurve *)MEM_callocN((blocks+1)*1024*sizeof(CutCurve), "MouseTrail");
			if (!curve) {
				printf("failed to re-allocate memory in get_mouse_trail()\n");
				return(NULL);
			}
			memcpy(curve, temp, blocks*1024*sizeof(CutCurve));
			blocks++;
			MEM_freeN(temp);
		}
	}

	bglFlush();
	glDrawBuffer(GL_BACK);
	persp(PERSP_VIEW);

	*len=i;

	return(curve);
}




/* ******************************************************************** */
/* Knife Subdivide Tool.  Subdivides edges intersected by a mouse trail
	drawn by user.
	
	Currently mapped to KKey when in MeshEdit mode.
	Usage:
		Hit Shift K, Select Centers or Exact
		Hold LMB down to draw path, hit RETKEY.
		ESC cancels as expected.
   
	Contributed by Robert Wenzlaff (Det. Thorn).
*/

/* prototype */
static float seg_intersect(struct EditEdge * e, CutCurve *c, int len, char mode, struct GHash *gh);

void KnifeSubdivide(char mode)
{
	EditMesh *em = G.editMesh;
	EditEdge *eed;
	EditVert *eve;
	CutCurve *curve;		
	Window *win;
	
	struct GHash *gh;
	int oldcursor, len=0;
	float isect=0.0;
	short numcuts=1;
	float  *scr, co[4];
	
	if (G.obedit==0) return;

	if (EM_nvertices_selected() < 2) {
		error("No edges are selected to operate on");
		return;
	}

	if (mode==KNIFE_PROMPT) {
		short val= pupmenu("Cut Type %t|Exact Line%x1|Midpoints%x2|Multicut%x3");
		if(val<1) return;
		mode = val;	// warning, mode is char, pupmenu returns -1 with ESC
	}

	if(mode == KNIFE_MULTICUT) {
		if(button(&numcuts, 2, 128, "Number of Cuts:")==0) return;
	}

	/* Set a knife cursor here */
	oldcursor=get_cursor();
	
	win=winlay_get_active_window();
	
	SetBlenderCursor(BC_KNIFECURSOR);
	
	for(eed=em->edges.first; eed; eed= eed->next) eed->tmp.fp = 0.0; /*store percentage of edge cut for KNIFE_EXACT here.*/
	
	/*the floating point coordinates of verts in screen space will be stored in a hash table according to the vertices pointer*/
	gh = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	for(eve=em->verts.first; eve; eve=eve->next){
		scr = MEM_mallocN(sizeof(float)*2, "Vertex Screen Coordinates");
		VECCOPY(co, eve->co);
		co[3]= 1.0;
		Mat4MulVec4fl(G.obedit->obmat, co);
		project_float(co,scr);
		BLI_ghash_insert(gh, eve, scr);
		eve->f1 = 0; /*store vertex intersection flag here*/
	
	}
	
	curve=get_mouse_trail(&len, TRAIL_MIXED, mode, gh);
	
	if (curve && len && mode){
		eed= em->edges.first;		
		while(eed) {	
			if( eed->v1->f & eed->v2->f & SELECT ){		// NOTE: uses vertex select, subdiv doesnt do edges yet
				isect=seg_intersect(eed, curve, len, mode, gh);
				if (isect) eed->f2= 1;
				else eed->f2=0;
				eed->tmp.fp= isect;
				//printf("isect=%i\n", isect);
			}
			else {
				eed->f2=0;
				eed->f1=0;
			}
			eed= eed->next;
		}
		
		if      (mode==KNIFE_EXACT)    esubdivideflag(1, 0, B_KNIFE|B_PERCENTSUBD,1,SUBDIV_SELECT_ORIG);
		else if (mode==KNIFE_MIDPOINT) esubdivideflag(1, 0, B_KNIFE,1,SUBDIV_SELECT_ORIG);
		else if (mode==KNIFE_MULTICUT) esubdivideflag(1, 0, B_KNIFE,numcuts,SUBDIV_SELECT_ORIG);

		eed=em->edges.first;
		while(eed){
			eed->f2=0;
			eed->f1=0;
			eed=eed->next;
		}	
	}
	/* Return to old cursor and flags...*/
	
	addqueue(curarea->win,  REDRAW, 0);
	window_set_cursor(win, oldcursor);
	BLI_ghash_free(gh, NULL, (GHashValFreeFP)MEM_freeN);
	if (curve) MEM_freeN(curve);

#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
#endif

	BIF_undo_push("Knife");
}

/* seg_intersect() Determines if and where a mouse trail intersects an EditEdge */

static float seg_intersect(EditEdge *e, CutCurve *c, int len, char mode, struct GHash *gh)
{
#define MAXSLOPE 100000
	float  x11, y11, x12=0, y12=0, x2max, x2min, y2max;
	float  y2min, dist, lastdist=0, xdiff2, xdiff1;
	float  m1, b1, m2, b2, x21, x22, y21, y22, xi;
	float  yi, x1min, x1max, y1max, y1min, perc=0; 
	float  *scr;
	float  threshold;
	int  i;
	
	//threshold = 0.000001; /*tolerance for vertex intersection*/
	threshold = G.scene->toolsettings->select_thresh / 100;
	
	/* Get screen coords of verts */
	scr = BLI_ghash_lookup(gh, e->v1);
	x21=scr[0];
	y21=scr[1];
	
	scr = BLI_ghash_lookup(gh, e->v2);
	x22=scr[0];
	y22=scr[1];
	
	xdiff2=(x22-x21);  
	if (xdiff2) {
		m2=(y22-y21)/xdiff2;
		b2= ((x22*y21)-(x21*y22))/xdiff2;
	}
	else {
		m2=MAXSLOPE;  /* Verticle slope  */
		b2=x22;      
	}
	
	/*check for *exact* vertex intersection first*/
	if(mode!=KNIFE_MULTICUT){
		for (i=0; i<len; i++){
			if (i>0){
				x11=x12;
				y11=y12;
			}
			else {
				x11=c[i].x;
				y11=c[i].y;
			}
			x12=c[i].x;
			y12=c[i].y;
			
			/*test e->v1*/
			if((x11 == x21 && y11 == y21) || (x12 == x21 && y12 == y21)){
				e->v1->f1 = 1;
				perc = 0;
				return(perc);
			}
			/*test e->v2*/
			else if((x11 == x22 && y11 == y22) || (x12 == x22 && y12 == y22)){
				e->v2->f1 = 1;
				perc = 0;
				return(perc);
			}
		}
	}
	
	/*now check for edge interesect (may produce vertex intersection as well)*/
	for (i=0; i<len; i++){
		if (i>0){
			x11=x12;
			y11=y12;
		}
		else {
			x11=c[i].x;
			y11=c[i].y;
		}
		x12=c[i].x;
		y12=c[i].y;
		
		/* Perp. Distance from point to line */
		if (m2!=MAXSLOPE) dist=(y12-m2*x12-b2);/* /sqrt(m2*m2+1); Only looking for */
						       /* change in sign.  Skip extra math */	
		else dist=x22-x12;	
		
		if (i==0) lastdist=dist;
		
		/* if dist changes sign, and intersect point in edge's Bound Box*/
		if ((lastdist*dist)<=0){
			xdiff1=(x12-x11); /* Equation of line between last 2 points */
			if (xdiff1){
				m1=(y12-y11)/xdiff1;
				b1= ((x12*y11)-(x11*y12))/xdiff1;
			}
			else{
				m1=MAXSLOPE;
				b1=x12;
			}
			x2max=MAX2(x21,x22)+0.001; /* prevent missed edges   */
			x2min=MIN2(x21,x22)-0.001; /* due to round off error */
			y2max=MAX2(y21,y22)+0.001;
			y2min=MIN2(y21,y22)-0.001;
			
			/* Found an intersect,  calc intersect point */
			if (m1==m2){ /* co-incident lines */
						/* cut at 50% of overlap area*/
				x1max=MAX2(x11, x12);
				x1min=MIN2(x11, x12);
				xi= (MIN2(x2max,x1max)+MAX2(x2min,x1min))/2.0;	
				
				y1max=MAX2(y11, y12);
				y1min=MIN2(y11, y12);
				yi= (MIN2(y2max,y1max)+MAX2(y2min,y1min))/2.0;
			}			
			else if (m2==MAXSLOPE){ 
				xi=x22;
				yi=m1*x22+b1;
			}
			else if (m1==MAXSLOPE){ 
				xi=x12;
				yi=m2*x12+b2;
			}
			else {
				xi=(b1-b2)/(m2-m1);
				yi=(b1*m2-m1*b2)/(m2-m1);
			}
			
			/* Intersect inside bounding box of edge?*/
			if ((xi>=x2min)&&(xi<=x2max)&&(yi<=y2max)&&(yi>=y2min)){
				/*test for vertex intersect that may be 'close enough'*/
				if(mode!=KNIFE_MULTICUT){
					if(xi <= (x21 + threshold) && xi >= (x21 - threshold)){
						if(yi <= (y21 + threshold) && yi >= (y21 - threshold)){
							e->v1->f1 = 1;
							perc = 0;
							break;
						}
					}
					if(xi <= (x22 + threshold) && xi >= (x22 - threshold)){
						if(yi <= (y22 + threshold) && yi >= (y22 - threshold)){
							e->v2->f1 = 1;
							perc = 0;
							break;
						}
					}
				}
				if ((m2<=1.0)&&(m2>=-1.0)) perc = (xi-x21)/(x22-x21);	
				else perc=(yi-y21)/(y22-y21); /*lower slope more accurate*/
				//isect=32768.0*(perc+0.0000153); /* Percentage in 1/32768ths */
				break;
			}
		}	
		lastdist=dist;
	}
	return(perc);
} 

void LoopMenu() /* Called by KKey */
{
	short ret;
	
	ret=pupmenu("Loop/Cut Menu %t|Loop Cut (CTRL-R)%x2|"
				"Knife (Exact) %x3|Knife (Midpoints)%x4|Knife (Multicut)%x5");
				
	switch (ret){
		case 2:
			CutEdgeloop(1);
			break;
		case 3: 
			KnifeSubdivide(KNIFE_EXACT);
			break;
		case 4:
			KnifeSubdivide(KNIFE_MIDPOINT);
			break;
		case 5:
			KnifeSubdivide(KNIFE_MULTICUT);
			break;
	}

}

