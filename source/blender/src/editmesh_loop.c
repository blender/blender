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

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
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

#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

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

#include "BSE_view.h"
#include "BSE_edit.h"
#include "BSE_drawview.h"

#include "BDR_drawobject.h"
#include "BDR_editobject.h"

#include "mydevice.h"
#include "blendef.h"

#include "editmesh.h"

/* next 2 includes for knife tool... shouldnt be! (ton) */
#include "GHOST_C-api.h"
#include "winlay.h"


/* *************** LOOP SELECT ************* */

short edgeFaces(EditEdge *e){
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
/* This function selects a vertex loop based on a each succesive edge having a valance of 4
   and not sharing a face with the previous edge */

/* It uses ->f flags still, which isn't causing bugs now, but better be put in ->f1 (ton) */

void vertex_loop_select() 
{
	EditMesh *em = G.editMesh;
	EditVert *v1=NULL,*v2=NULL;
	EditEdge *search=NULL,*startEdge=NULL,*valSearch = NULL,*nearest = NULL,*compEdge;
	EditEdge *EdgeVal[5] = {NULL,NULL,NULL,NULL,NULL};
	short numEdges=0,curEdge = 0,looking = 1,edgeValCount = 0,i=0,looped = 0,choosing = 1,event,noloop=0,cancel=0, val;
	short protect = 0, dist= 50;
	short mvalo[2] = {0,0}, mval[2];

	SetBlenderCursor(BC_VLOOPCURSOR);
	for(search=em->edges.first;search;search=search->next)
		numEdges++;

	/* start with v1 and go in one direction. */
	while(choosing){
		getmouseco_areawin(mval);
		if (mval[0] != mvalo[0] || mval[1] != mvalo[1]) {

			mvalo[0] = mval[0];
			mvalo[1] = mval[1];

			dist= 50;
			nearest = findnearestedge(&dist);	// returns actual distance in dist
			
			scrarea_do_windraw(curarea);	// after findnearestedge, backbuf!

			if (nearest && edgeFaces(nearest)==2) {
				for(search = em->edges.first;search;search=search->next)
					search->f &= ~32;
					
				compEdge = startEdge = nearest;
				nearest->f |= 32;
				curEdge = 0;
				v1 = startEdge->v1;
				v2 = startEdge->v2;
				looking = 1;
				while(looking){
					if(protect++ > numEdges) break;
					if(edgeFaces(compEdge) != 2) break;
					/*Find Edges that have v1*/
					edgeValCount = -1;
					EdgeVal[0] = EdgeVal[1] = EdgeVal[2] = NULL;
					
					for(valSearch = em->edges.first;valSearch;valSearch = valSearch->next){
						if(valSearch->v1 == v1 || valSearch->v2 == v1){
							if(valSearch != compEdge){
								if((valSearch->v1->h == 0) && (valSearch->v2->h == 0)){
									if(edgeFaces(valSearch) == 2){
										edgeValCount++;							
										EdgeVal[edgeValCount] = valSearch;
									}
								}
							}
						}
						if(edgeValCount == 3)break;
					}
					/* Check that there was a valance of 4*/
					if(edgeValCount != 2){
						noloop = 1;
						looking = 0;
						break;
					}
					else{
					/* There were 3 edges, so find the one that does not share the previous edge */
						for(i=0;i<3;i++){
							if(sharesFace(compEdge,EdgeVal[i]) == 0){
								/* We went all the way around the loop */
								if(EdgeVal[i] == nearest){
									looking = 0;
									looped = 1;
									break;
								}
								else{
									/* we are still in the loop, so add the next edge*/
									curEdge++;
									EdgeVal[i]->f |= 32;
									compEdge = EdgeVal[i];
									if(compEdge->v1 == v1)
										v1 = compEdge->v2;
									else
										v1 = compEdge->v1;
								}
							}
						}
					}
				}	
				compEdge = nearest;
				looking = 1;
				protect = 0;
				while(looking/* && !looped*/){
					if(protect++ > numEdges) break;
					if(edgeFaces(compEdge) != 2) break;
					/*Find Edges that have v1*/
					edgeValCount = -1;
					EdgeVal[0] = EdgeVal[1] = EdgeVal[2] = NULL;
					
					for(valSearch = em->edges.first;valSearch;valSearch = valSearch->next){
						if(valSearch->v1 == v2 || valSearch->v2 == v2){
							if(valSearch != compEdge){
								if((valSearch->v1->h == 0) && (valSearch->v2->h == 0)){
									if(edgeFaces(valSearch) == 2){
										edgeValCount++;							
										EdgeVal[edgeValCount] = valSearch;
									}
								}
							}
						}
						if(edgeValCount == 3)break;
					}
					/* Check that there was a valance of 4*/
					if(edgeValCount != 2){
						noloop = 1;
						looking = 0;
						break;
					}
					else{
					/* There were 3 edges, so find the one that does not share the previous edge */
						for(i=0;i<3;i++){
							if(sharesFace(compEdge,EdgeVal[i]) == 0){
								/* We went all the way around the loop */
								if(EdgeVal[i] == nearest){
									looking = 0;
									looped = 1;
									break;
								}
								else{
									/* we are still in the loop, so add the next edge*/
									curEdge++;
									EdgeVal[i]->f |= 32;
									compEdge = EdgeVal[i];
									if(compEdge->v1 == v2)
										v2 = compEdge->v2;
									else
										v2 = compEdge->v1;
								}
							}
						}
					}
				}
				/* set up for opengl drawing in the 3d window */
				persp(PERSP_VIEW);
				glPushMatrix();
				mymultmatrix(G.obedit->obmat);
				glColor3ub(0, 255, 255);
				for(search = em->edges.first;search;search= search->next){
					if(search->f & 32){
						glBegin(GL_LINES);			
						glVertex3f(search->v1->co[0],search->v1->co[1],search->v1->co[2]);
						glVertex3f(search->v2->co[0],search->v2->co[1],search->v2->co[2]);
						glEnd();
					}
				}		

				glPopMatrix();
			}
		}

		screen_swapbuffers();

		/* backbuffer refresh for non-apples (no aux) */
#ifndef __APPLE__
		if(G.vd->drawtype>OB_WIRE && (G.vd->flag & V3D_ZBUF_SELECT)) {
			backdrawview3d(0);
		}
#endif
		while(qtest()) 
		{
			val=0;
			event= extern_qread(&val);
			if(val && ((event==LEFTMOUSE || event==RETKEY) || event == MIDDLEMOUSE))
			{
				if (nearest==NULL)
					cancel = 1;
				choosing=0;
				break;
			}
			if(val && (event==ESCKEY || event==RIGHTMOUSE ))
			{
				choosing=0;
				cancel = 1;
				break;
			}
			if(val && (event==BKEY && G.qual==LR_ALTKEY ))
			{			
				
				SetBlenderCursor(SYSCURSOR);
				loopoperations(LOOP_SELECT);
				return;
			}
		}
	}
	if(!cancel){
		/* If this is a unmodified select, clear the selection */
		
		/* XXX note that !1 is 0, so it not only clears bit 1 (ton) */
		if(!(G.qual & LR_SHIFTKEY) && !(G.qual & LR_ALTKEY)){
			for(search = em->edges.first;search;search= search->next){
				search->v1->f &= !1;
				search->v2->f &= !1;
			}
			EM_clear_flag_all(SELECT);	/* XXX probably that's sufficient */
		}
		/* Alt was not pressed, so add to the selection */
		if(!(G.qual & LR_ALTKEY)){
			for(search = em->edges.first;search;search= search->next){
				if(search->f & 32){
					search->v1->f |= 1;
					search->v2->f |= 1;
				}
				search->f &= ~32;
			}
			/* XXX this will correctly flush */
		}
		/* alt was pressed, so subtract from the selection */
		else
		{
			/* XXX this doesnt flush correct in face select mode */
			for(search = em->edges.first;search;search= search->next){
				if(search->f & 32){
					search->v1->f &= !1;
					search->v2->f &= !1;
					EM_select_edge(search, 0);	// the call to deselect edge
				}
				search->f &= ~32;
			}
		}
		
		EM_select_flush(); // flushes vertex -> edge -> face selection
		BIF_undo_push("Select Vertex Loop");
	}

	addqueue(curarea->win, REDRAW, 1); 
	SetBlenderCursor(SYSCURSOR);
	return;
}

/* *********** END LOOP SELECT ********** */



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
	short  x; 
	short  y;
} CutCurve;

CutCurve *get_mouse_trail(int *len, char mode){

	CutCurve *curve,*temp;
	short event, val, ldown=0, restart=0, rubberband=0;
	short  mval[2], lockaxis=0, lockx=0, locky=0, lastx=0, lasty=0;
	int i=0, j, blocks=1, lasti=0;
	
	*len=0;
	curve=(CutCurve *)MEM_callocN(1024*sizeof(CutCurve), "MouseTrail");

	if (!curve) {
		printf("failed to allocate memory in get_mouse_trail()\n");
		return(NULL);
	}
	mywinset(curarea->win);
	glDrawBuffer(GL_FRONT);
	
	headerprint("LMB to draw, Enter to finish, ESC to abort.");

	persp(PERSP_WIN);
	
	glColor3ub(200, 200, 0);
	
	while(TRUE) {
		
		event=extern_qread(&val);	/* Enter or RMB indicates finish */
		if(val) {
			if(event==RETKEY || event==PADENTER) break;
		}
		
		if( event==ESCKEY || event==RIGHTMOUSE ) {
			if (curve) MEM_freeN(curve);
			*len=0;
			glFlush();
			glDrawBuffer(GL_BACK);
			return(NULL);
			break;
		}	
		
		if (rubberband)  { /* rubberband mode, undraw last rubberband */
			glLineWidth(2.0);
			sdrawXORline(curve[i-1].x, curve[i-1].y,mval[0], mval[1]); 
			glLineWidth(1.0);
			glFlush();
			rubberband=0;
		}
		
		getmouseco_areawin(mval);
		
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
				for(j=1;j<i;j++) sdrawXORline(curve[j-1].x, curve[j-1].y, curve[j].x, curve[j].y);
				if (rubberband) sdrawXORline(curve[j].x, curve[j].y, mval[0], mval[1]);
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
			sdrawline(curve[i-2].x, curve[i-2].y, curve[i-1].x, curve[i-1].y);
			glFlush();
		}
		
		if ((i==lasti)&&(i>0)) { /*Draw rubberband */
			glLineWidth(2.0);
			sdrawXORline(curve[i-1].x, curve[i-1].y,mval[0], mval[1]);
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

	glFlush();
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
short seg_intersect(struct EditEdge * e, CutCurve *c, int len);

void KnifeSubdivide(char mode)
{
	EditMesh *em = G.editMesh;
	int oldcursor, len=0;
	short isect=0;
	CutCurve *curve;		
	EditEdge *eed; 
	Window *win;	
	
	if (G.obedit==0) return;

	if (EM_nvertices_selected() < 2) {
		error("No edges are selected to operate on");
		return;
	}

	if (mode==KNIFE_PROMPT) {
		short val= pupmenu("Cut Type %t|Exact Line%x1|Midpoints%x2");
		if(val<1) return;
		mode= val;	// warning, mode is char, pupmenu returns -1 with ESC
	}

	calc_meshverts_ext();  /*Update screen coords for current window */
	
	/* Set a knife cursor here */
	oldcursor=get_cursor();

	win=winlay_get_active_window();
	
	SetBlenderCursor(BC_KNIFECURSOR);
	
	curve=get_mouse_trail(&len, TRAIL_MIXED);
	
	if (curve && len && mode){
		eed= em->edges.first;		
		while(eed) {	
			if( eed->f & SELECT ){
				isect=seg_intersect(eed, curve, len);
				if (isect) eed->f2= 1;
				else eed->f2=0;
				eed->f1= isect;
				//printf("isect=%i\n", isect);
			}
			else {
				eed->f2=0;
				eed->f1=0;
			}
			eed= eed->next;
		}
		
		if (mode==1) subdivideflag(1, 0, B_KNIFE|B_PERCENTSUBD);
		else if (mode==2) subdivideflag(1, 0, B_KNIFE);
		
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
	if (curve) MEM_freeN(curve);

	BIF_undo_push("Knife");
}

/* seg_intersect() Determines if and where a mouse trail intersects an EditEdge */

short seg_intersect(EditEdge *e, CutCurve *c, int len){
#define MAXSLOPE 100000
	short isect=0;
	float  x11, y11, x12=0, y12=0, x2max, x2min, y2max;
	float  y2min, dist, lastdist=0, xdiff2, xdiff1;
	float  m1, b1, m2, b2, x21, x22, y21, y22, xi;
	float  yi, x1min, x1max, y1max, y1min, perc=0; 
	float  scr[2], co[4];
	int  i;
	
	/* Get screen coords of verts (v->xs and v->ys clip if off screen */
	VECCOPY(co, e->v1->co);
	co[3]= 1.0;
	Mat4MulVec4fl(G.obedit->obmat, co);
	project_float(co, scr);
	x21=scr[0];
	y21=scr[1];
	
	VECCOPY(co, e->v2->co);
	co[3]= 1.0;
	Mat4MulVec4fl(G.obedit->obmat, co);
	project_float(co, scr);
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
			if (m1==m2){ 		/* co-incident lines */
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
				if ((m2<=1.0)&&(m2>=-1.0)) perc = (xi-x21)/(x22-x21);	
				else perc=(yi-y21)/(y22-y21); /*lower slope more accurate*/
				isect=32768.0*(perc+0.0000153); /* Percentage in 1/32768ths */
				break;
			}
		}	
		lastdist=dist;
	}
	return(isect);
} 

/* ******************** LOOP ******************************************* */

/* XXX: this loop function is totally out of control!
   can be half the code, and using structured functions (ton) */
   
/* 
functionality: various loop functions
parameters: mode tells the function what it should do with the loop:
		LOOP_SELECT = select
		LOOP_CUT = cut in half
*/	

void loopoperations(char mode)
{
	EditMesh *em = G.editMesh;
	EditVert* look = NULL;
	EditEdge *start, *eed, *opposite,*currente, *oldstart;
	EditEdge **tagged = NULL,**taggedsrch = NULL,*close;
	EditFace *efa,**percentfacesloop = NULL, *currentvl,  *formervl;	
	short lastface=0, foundedge=0, c=0, tri=0, side=1, totface=0, searching=1, event=0, noface=1;
	short skip,nextpos,percentfaces, dist=50;

	int i=0,ect=0,j=0,k=0,cut,smooth,timesthrough=0,inset = 0;

	float percentcut, outcut;

	char mesg[100];

	if ((G.obedit==0) || (em->faces.first==0)) return;
	
	SetBlenderCursor(BC_VLOOPCURSOR);

	/* Clear flags */
	for(eed=em->edges.first; eed; eed=eed->next) eed->f2= 0;
	for(efa= em->faces.first; efa; efa=efa->next) efa->f1= 0;
	
	start=NULL;
	oldstart=NULL;

	while(searching){
		
		/* reset variables */
		start=eed=opposite=currente=0;
		efa=currentvl=formervl=0;
		side=noface=1;
		lastface=foundedge=c=tri=totface=0;		
				
		//start=findnearestvisibleedge();
		dist= 50;
		start= findnearestedge(&dist);
		
		/* used flags in the code:
		   vertex->f & 2: in findnearestvisibleedge
		   edge->f2 : subdiv codes
		   efa->f1 : subdiv codes
		*/
		
		/* If the edge doesn't belong to a face, it's not a valid starting edge */
		/* and only accept starting edge if it is part of at least one visible face */
		if(start){
			start->f2 |= 16;
			efa=em->faces.first;
			while(efa){
				/* since this edge is on the face, check if the face is hidden */
				if( efa->h==0  ){
					if(efa->e1->f2 & 16){
						noface=0;
						efa->e1->f2 &= ~16;
					}
					else if(efa->e2->f2 & 16){					
						noface=0;
						efa->e2->f2 &= ~16;
					}
					else if(efa->e3->f2 & 16){					
						noface=0;
						efa->e3->f2 &= ~16;
					}
					else if(efa->e4 && (efa->e4->f2 & 16)){					
						noface=0;
						efa->e4->f2 &= ~16;
					}
				}
				efa=efa->next;
			}			
		}
				
		/* Did we find anything that is selectable? */
		if(start && !noface && (oldstart==NULL || start!=oldstart)){
					
			/* If we stay in the neighbourhood of this edge, we don't have to recalculate the loop everytime*/
			oldstart=start;	
			
			/* Clear flags */
			for(eed=em->edges.first; eed; eed=eed->next){			
				eed->f2 &= ~(2|4|8|32|64);
				eed->v1->f &= ~(2|8|16); // xxxx
				eed->v2->f &= ~(2|8|16);				
			}
			
			for(efa= em->faces.first; efa; efa=efa->next){			
				efa->f1 &= ~(4|8);
				totface++;				
			}
					
			/* Tag the starting edge */
			start->f2 |= (2|4|8|64);				
			start->v1->f |= 2;  /* xxxx */
			start->v2->f |= 2;		
			
			currente=start;						
			
			/*-----Limit the Search----- */
			while(!lastface && c<totface+1){
				
				/*----------Get Loop------------------------*/
				tri=foundedge=lastface=0;													
				efa= em->faces.first;		
				while(efa && !foundedge && !tri){
									
					if(!(efa->v4)){	/* Exception for triangular faces */
						
						if((efa->e1->f2 | efa->e2->f2 | efa->e3->f2) & 2){
							if(!(efa->f1 & 4)){								
								tri=1;
								currentvl=efa;
								if(side==1) efa->f1 |= 4;
							}
						}						
					}
					else{
						
						if((efa->e1->f2 | efa->e2->f2 | efa->e3->f2 | efa->e4->f2) & 2){
							
							if(c==0){	/* just pick a face, doesn't matter wich side of the edge we go to */
								if(!(efa->f1 & 4)){
									
									if(!(efa->e1->v1->f & 2) && !(efa->e1->v2->f & 2)){ // xxxxx
										if(efa->e1->h==0){
											opposite=efa->e1;														
											foundedge=1;
										}
									}
									else if(!(efa->e2->v1->f & 2) && !(efa->e2->v2->f & 2)){ // xxxx
										if(efa->e2->h==0){
											opposite=efa->e2;
											foundedge=1;
										}
									}
									else if(!(efa->e3->v1->f & 2) && !(efa->e3->v2->f & 2)){
										if(efa->e3->h==0){
											opposite=efa->e3;
											foundedge=1;
										}
									}
									else if(!(efa->e4->v1->f & 2) && !(efa->e4->v2->f & 2)){
										if(efa->e4->h==0){
											opposite=efa->e4;
											foundedge=1;
										}
									}
									
									if(foundedge){
										currentvl=efa;
										formervl=efa;
									
										/* mark this side of the edge so we know in which direction we went */
										if(side==1) efa->f1 |= 4;
									}
								}
							}
							else {	
								if(efa!=formervl){	/* prevent going backwards in the loop */
								
									if(!(efa->e1->v1->f & 2) && !(efa->e1->v2->f & 2)){
										if(efa->e1->h==0){
											opposite=efa->e1;														
											foundedge=1;
										}
									}
									else if(!(efa->e2->v1->f & 2) && !(efa->e2->v2->f & 2)){
										if(efa->e2->h==0){
											opposite=efa->e2;
											foundedge=1;
										}
									}
									else if(!(efa->e3->v1->f & 2) && !(efa->e3->v2->f & 2)){
										if(efa->e3->h==0){
											opposite=efa->e3;
											foundedge=1;
										}
									}
									else if(!(efa->e4->v1->f & 2) && !(efa->e4->v2->f & 2)){
										if(efa->e4->h==0){
											opposite=efa->e4;
											foundedge=1;
										}
									}
									
									currentvl=efa;
								}
							}
						}
					}
				efa=efa->next;
				}
				/*----------END Get Loop------------------------*/
				
			
				/*----------Decisions-----------------------------*/
				if(foundedge){
					/* mark the edge and face as done */					
					currente->f2 |= 8;
					currentvl->f1 |= 8;

					if(opposite->f2 & 4) lastface=1;	/* found the starting edge! close loop */								
					else{
						/* un-set the testflags */
						currente->f2 &= ~2;
						currente->v1->f &= ~2; // xxxx
						currente->v2->f &= ~2;							
						
						/* set the opposite edge to be the current edge */				
						currente=opposite;							
						
						/* set the current face to be the FORMER face (to prevent going backwards in the loop) */
						formervl=currentvl;
						
						/* set the testflags */
						currente->f2 |= 2;
						currente->v1->f |= 2; // xxxx
						currente->v2->f |= 2;			
					}
					c++;
				}
				else{	
					/* un-set the testflags */
					currente->f2 &= ~2;
					currente->v1->f &= ~2; // xxxx
					currente->v2->f &= ~2;
					
					/* mark the edge and face as done */
					currente->f2 |= 8;
					currentvl->f1 |= 8;
					
					
												
					/* is the the first time we've ran out of possible faces?
					*  try to start from the beginning but in the opposite direction go as far as possible
					*/				
					if(side==1){						
						if(tri)tri=0;
						currente=start;
						currente->f2 |= 2;
						currente->v1->f |= 2; // xxxx
						currente->v2->f |= 2;					
						side++;
						c=0;
					}
					else lastface=1;
				}				
				/*----------END Decisions-----------------------------*/
				
			}
			/*-----END Limit the Search----- */
			
			
			/*------------- Preview lines--------------- */
			
			/* uses callback mechanism to draw it all in current area */
			scrarea_do_windraw(curarea);			
			
			/* set window matrix to perspective, default an area returns with buttons transform */
			persp(PERSP_VIEW);
			/* make a copy, for safety */
			glPushMatrix();
			/* multiply with the object transformation */
			mymultmatrix(G.obedit->obmat);
			
			glColor3ub(255, 255, 0);
			
			if(mode==LOOP_SELECT){
				efa= em->faces.first;
				while(efa){
					if(efa->f1 & 8){
						
						if(!(efa->e1->f2 & 8)){
							glBegin(GL_LINES);							
							glVertex3fv(efa->e1->v1->co);
							glVertex3fv(efa->e1->v2->co);
							glEnd();	
						}
						
						if(!(efa->e2->f2 & 8)){
							glBegin(GL_LINES);							
							glVertex3fv(efa->e2->v1->co);
							glVertex3fv(efa->e2->v2->co);
							glEnd();	
						}
						
						if(!(efa->e3->f2 & 8)){
							glBegin(GL_LINES);							
							glVertex3fv(efa->e3->v1->co);
							glVertex3fv(efa->e3->v2->co);
							glEnd();	
						}
						
						if(efa->e4){
							if(!(efa->e4->f2 & 8)){
								glBegin(GL_LINES);							
								glVertex3fv(efa->e4->v1->co);
								glVertex3fv(efa->e4->v2->co);
								glEnd();	
							}
						}
					}
					efa=efa->next;
				}
			}
				
			if(mode==LOOP_CUT){
				efa= em->faces.first;
				while(efa){
					if(efa->f1 & 8){
						float cen[2][3];
						int a=0;						
						
						efa->v1->f &= ~8; // xxx
						efa->v2->f &= ~8;
						efa->v3->f &= ~8;
						if(efa->v4)efa->v4->f &= ~8;
					
						if(efa->e1->f2 & 8){
							cen[a][0]= (efa->e1->v1->co[0] + efa->e1->v2->co[0])/2.0;
							cen[a][1]= (efa->e1->v1->co[1] + efa->e1->v2->co[1])/2.0;
							cen[a][2]= (efa->e1->v1->co[2] + efa->e1->v2->co[2])/2.0;
							
							efa->e1->v1->f |= 8; // xxx
							efa->e1->v2->f |= 8;
							
							a++;
						}
						if((efa->e2->f2 & 8) && a!=2){
							cen[a][0]= (efa->e2->v1->co[0] + efa->e2->v2->co[0])/2.0;
							cen[a][1]= (efa->e2->v1->co[1] + efa->e2->v2->co[1])/2.0;
							cen[a][2]= (efa->e2->v1->co[2] + efa->e2->v2->co[2])/2.0;
							
							efa->e2->v1->f |= 8; // xxx
							efa->e2->v2->f |= 8;
							
							a++;
						}
						if((efa->e3->f2 & 8) && a!=2){
							cen[a][0]= (efa->e3->v1->co[0] + efa->e3->v2->co[0])/2.0;
							cen[a][1]= (efa->e3->v1->co[1] + efa->e3->v2->co[1])/2.0;
							cen[a][2]= (efa->e3->v1->co[2] + efa->e3->v2->co[2])/2.0;
							
							efa->e3->v1->f |= 8; // xxx
							efa->e3->v2->f |= 8;
							
							a++;
						}
						
						if(efa->e4){
							if((efa->e4->f2 & 8) && a!=2){
								cen[a][0]= (efa->e4->v1->co[0] + efa->e4->v2->co[0])/2.0;
								cen[a][1]= (efa->e4->v1->co[1] + efa->e4->v2->co[1])/2.0;
								cen[a][2]= (efa->e4->v1->co[2] + efa->e4->v2->co[2])/2.0;
								
								efa->e4->v1->f |= 8; // xxx
								efa->e4->v2->f |= 8;
							
								a++;
							}
						}
						else{	/* if it's a triangular face, set the remaining vertex as the cutcurve coordinate */														
								if(!(efa->v1->f & 8) && efa->v1->h==0){ // xxx
									cen[a][0]= efa->v1->co[0];
									cen[a][1]= efa->v1->co[1];
									cen[a][2]= efa->v1->co[2];
									a++;								
								}
								else if(!(efa->v2->f & 8) && efa->v2->h==0){
									cen[a][0]= efa->v2->co[0];
									cen[a][1]= efa->v2->co[1];
									cen[a][2]= efa->v2->co[2];	
									a++;
								}
								else if(!(efa->v3->f & 8) && efa->v3->h==0){
									cen[a][0]= efa->v3->co[0];
									cen[a][1]= efa->v3->co[1];
									cen[a][2]= efa->v3->co[2];
									a++;									
								}							
						}
						
						if(a==2){
							glBegin(GL_LINES);
							
							glVertex3fv(cen[0]);
							glVertex3fv(cen[1]);	
												
							glEnd();
						}						
					}
					efa=efa->next;
				}
				
				eed=em->edges.first; 
				while(eed){
					if(eed->f2 & 64){
						glBegin(GL_LINES);
						glColor3ub(200, 255, 200);
						glVertex3fv(eed->v1->co);
						glVertex3fv(eed->v2->co);
						glEnd();
						eed=0;
					}else{
						eed = eed->next;
					}
				}		
			}
			
			/* restore matrix transform */
			glPopMatrix();
			
			headerprint("LMB to confirm, RMB to cancel");
			
			/* this also verifies other area/windows for clean swap */
			screen_swapbuffers();
			
			/* backbuffer refresh for non-apples (no aux) */
#ifndef __APPLE__
			if(G.vd->drawtype>OB_WIRE && (G.vd->flag & V3D_ZBUF_SELECT)) {
				backdrawview3d(0);
			}
#endif
	
			/*--------- END Preview Lines------------*/
				
		}/*if(start!=NULL){ */
		
		while(qtest()) {
			unsigned short val=0;			
			event= extern_qread(&val);	/* extern_qread stores important events for the mainloop to handle */

			/* val==0 on key-release event */
			if(val && (event==ESCKEY || event==RIGHTMOUSE || event==LEFTMOUSE || event==RETKEY || event == MIDDLEMOUSE)){
				searching=0;
			}
		}	
		
	}/*while(event!=ESCKEY && event!=RIGHTMOUSE && event!=LEFTMOUSE && event!=RETKEY){*/
	
	/*----------Select Loop------------*/
	if(mode==LOOP_SELECT && start!=NULL && ((event==LEFTMOUSE || event==RETKEY) || event == MIDDLEMOUSE || event == BKEY)){
				
		/* If this is a unmodified select, clear the selection */
		if(!(G.qual & LR_SHIFTKEY) && !(G.qual & LR_ALTKEY)){
			for(efa= em->faces.first;efa;efa=efa->next){
				EM_select_face(efa, 0);	// and this is correct deselect face		
			}
		}
		/* Alt was not pressed, so add to the selection */
		if(!(G.qual & LR_ALTKEY)){
			for(efa= em->faces.first;efa;efa=efa->next){
				if(efa->f1 & 8){
					EM_select_face(efa, 1);	// and this is correct select face
				}
			}
		}
		/* alt was pressed, so subtract from the selection */
		else
		{
			for(efa= em->faces.first;efa;efa=efa->next){
				if(efa->f1 & 8){
					EM_select_face(efa, 0);	// this is correct deselect face
				}
			}
		}
	
	}
	/*----------END Select Loop------------*/
	
	/*----------Cut Loop---------------*/			
	if(mode==LOOP_CUT && start!=NULL && (event==LEFTMOUSE || event==RETKEY)){
		
		/* count the number of edges in the loop */		
		for(eed=em->edges.first; eed; eed = eed->next){
			if(eed->f2 & 8)
				ect++;
		}		
		
		tagged = MEM_mallocN(ect*sizeof(EditEdge*), "tagged");
		taggedsrch = MEM_mallocN(ect*sizeof(EditEdge*), "taggedsrch");
		for(i=0;i<ect;i++)
		{
			tagged[i] = NULL;
			taggedsrch[i] = NULL;
		}
		ect = 0;
		for(eed=em->edges.first; eed; eed = eed->next){
			if(eed->f2 & 8)
			{
				if(eed->h==0){
					eed->v1->f |= SELECT;
					eed->v2->f |= SELECT;
					eed->f |= SELECT;
					tagged[ect] = eed;
					eed->f2 &= ~(32);
					ect++;
				}
			}			
		}
		taggedsrch[0] = tagged[0];

		while(timesthrough < 2)
		{
			i=0;
			while(i < ect){/*Look at the members of the search array to line up cuts*/
				if(taggedsrch[i]==NULL)break;
				for(j=0;j<ect;j++){			 /*Look through the list of tagged verts for connected edges*/
					int addededge = 0;
					if(taggedsrch[i]->f2 & 32)        /*If this edgee is marked as flipped, use vert 2*/
						look = taggedsrch[i]->v2;
					else							 /*else use vert 1*/
						look = taggedsrch[i]->v1;

					if(taggedsrch[i] == tagged[j])
						continue;  /*If we are looking at the same edge, skip it*/
	
					skip = 0;
					for(k=0;k<ect;k++)	{
						if(taggedsrch[k] == NULL)	/*go to empty part of search list without finding*/
							break;							
						if(tagged[j] == taggedsrch[k]){		/*We found a match already in the list*/
							skip = 1;
							break;
						}
					}
					if(skip)
						continue;
					nextpos = 0;
					if(findedgelist(look,tagged[j]->v2)){
						while(nextpos < ect){ /*Find the first open spot in the search array*/
							if(taggedsrch[nextpos] == NULL){
								taggedsrch[nextpos] = tagged[j]; /*put tagged[j] in it*/
								taggedsrch[nextpos]->f2 |= 32;
								addededge = 1;
								break;
							}
							else
								nextpos++;
						}
					} /* End else if connected to vert 2*/
					else if(findedgelist(look,tagged[j]->v1)){   /*If our vert is connected to vert 1 */
						while(nextpos < ect){ /*Find the first open spot in the search array */
							if(taggedsrch[nextpos] == NULL){
								taggedsrch[nextpos] = tagged[j]; /*put tagged[j] in it*/
								addededge = 1;
								break;
							}
							else 
								nextpos++;
						}
					}

					if(addededge)
					{
						break;
					}					
				}/* End Outer For (j)*/
				i++;
			} /* End while(j<ect)*/
			timesthrough++;
		} /*end while timesthrough */
		percentcut = 0.50;
		searching = 1;
		cut   = 1;
		smooth = 0;
		close = NULL;


		/* Count the Number of Faces in the selected loop*/
		percentfaces = 0;
		for(efa= em->faces.first; efa ;efa=efa->next){
			if(efa->f1 & 8)
			 {
				percentfaces++;	
			 }
		}
			
		/* create a dynamic array for those face pointers */
		percentfacesloop = MEM_mallocN(percentfaces*sizeof(EditFace*), "percentage");

		/* put those faces in the array */
		i=0;
		for(efa= em->faces.first; efa ;efa=efa->next){
			 if(efa->f1 & 8)
			 {
				percentfacesloop[i] = efa;	
				i++;
			 }
		}

		while(searching){
			
			/* For the % calculation */
			short mval[2];			
			float labda, rc[2], len, slen=0.0;
			float v1[2], v2[2], v3[2];

			/*------------- Percent Cut Preview Lines--------------- */
			scrarea_do_windraw(curarea);			
			persp(PERSP_VIEW);
			glPushMatrix();
			mymultmatrix(G.obedit->obmat);
			glColor3ub(0, 255, 255);
				
			/*Put the preview lines where they should be for the percentage selected.*/

			for(i=0;i<percentfaces;i++){
				efa = percentfacesloop[i];
				for(eed = em->edges.first; eed; eed=eed->next){
					if(eed->f2 & 64){	/* color the starting edge */			
						glBegin(GL_LINES);
												
						glColor3ub(200, 255, 200);
						glVertex3fv(eed->v1->co);					
						glVertex3fv(eed->v2->co);
						
						glEnd();

						glPointSize(5);
						glBegin(GL_POINTS);
						glColor3ub(255,0,255);
						
						if(eed->f2 & 32)
							glVertex3fv(eed->v2->co);			
						else
							glVertex3fv(eed->v1->co);
						glEnd();


						/*Get Starting Edge Length*/
						slen = sqrt((eed->v1->co[0]-eed->v2->co[0])*(eed->v1->co[0]-eed->v2->co[0])+
									(eed->v1->co[1]-eed->v2->co[1])*(eed->v1->co[1]-eed->v2->co[1])+
									(eed->v1->co[2]-eed->v2->co[2])*(eed->v1->co[2]-eed->v2->co[2]));
					}
				}
				
				if(!inset){
					glColor3ub(0,255,255);
					if(efa->f1 & 8)
					{
						float cen[2][3];
						int a=0;					
						
						efa->v1->f &= ~8; // xxx
						efa->v2->f &= ~8;
						efa->v3->f &= ~8;
						if(efa->v4)efa->v4->f &= ~8;
						
						if(efa->e1->f2 & 8){
							float pct;
							if(efa->e1->f2 & 32)
								pct = 1-percentcut;
							else
								pct = percentcut;
							cen[a][0]= efa->e1->v1->co[0] - ((efa->e1->v1->co[0] - efa->e1->v2->co[0]) * (pct));
							cen[a][1]= efa->e1->v1->co[1] - ((efa->e1->v1->co[1] - efa->e1->v2->co[1]) * (pct));
							cen[a][2]= efa->e1->v1->co[2] - ((efa->e1->v1->co[2] - efa->e1->v2->co[2]) * (pct));
							efa->e1->v1->f |= 8; // xxx
							efa->e1->v2->f |= 8;
							a++;
						}
						if((efa->e2->f2 & 8) && a!=2)
						{
							float pct;
							if(efa->e2->f2 & 32)
								pct = 1-percentcut;
							else
								pct = percentcut;
							cen[a][0]= efa->e2->v1->co[0] - ((efa->e2->v1->co[0] - efa->e2->v2->co[0]) * (pct));
							cen[a][1]= efa->e2->v1->co[1] - ((efa->e2->v1->co[1] - efa->e2->v2->co[1]) * (pct));
							cen[a][2]= efa->e2->v1->co[2] - ((efa->e2->v1->co[2] - efa->e2->v2->co[2]) * (pct));

							efa->e2->v1->f |= 8; // xxx
							efa->e2->v2->f |= 8;
							
							a++;
						}
						if((efa->e3->f2 & 8) && a!=2){
							float pct;
							if(efa->e3->f2 & 32)
								pct = 1-percentcut;
							else
								pct = percentcut;
							cen[a][0]= efa->e3->v1->co[0] - ((efa->e3->v1->co[0] - efa->e3->v2->co[0]) * (pct));
							cen[a][1]= efa->e3->v1->co[1] - ((efa->e3->v1->co[1] - efa->e3->v2->co[1]) * (pct));
							cen[a][2]= efa->e3->v1->co[2] - ((efa->e3->v1->co[2] - efa->e3->v2->co[2]) * (pct));

							efa->e3->v1->f |= 8; // xxx
							efa->e3->v2->f |= 8;
							
							a++;
						}
							
						if(efa->e4){
							if((efa->e4->f2 & 8) && a!=2){
								float pct;
								if(efa->e4->f2 & 32)
									pct = 1-percentcut;
								else
									pct = percentcut;
								cen[a][0]= efa->e4->v1->co[0] - ((efa->e4->v1->co[0] - efa->e4->v2->co[0]) * (pct));
								cen[a][1]= efa->e4->v1->co[1] - ((efa->e4->v1->co[1] - efa->e4->v2->co[1]) * (pct));
								cen[a][2]= efa->e4->v1->co[2] - ((efa->e4->v1->co[2] - efa->e4->v2->co[2]) * (pct));

								efa->e4->v1->f |= 8; // xxx
								efa->e4->v2->f |= 8;
							
								a++;
							}
						}
						else {	/* if it's a triangular face, set the remaining vertex as the cutcurve coordinate */
							if(!(efa->v1->f & 8) && efa->v1->h==0){ // xxx
								cen[a][0]= efa->v1->co[0];
								cen[a][1]= efa->v1->co[1];
								cen[a][2]= efa->v1->co[2];
								a++;								
							}
							else if(!(efa->v2->f & 8) && efa->v2->h==0){ // xxx
								cen[a][0]= efa->v2->co[0];
								cen[a][1]= efa->v2->co[1];
								cen[a][2]= efa->v2->co[2];
								a++;								
							}
							else if(!(efa->v3->f & 8) && efa->v3->h==0){ // xxx
								cen[a][0]= efa->v3->co[0];
								cen[a][1]= efa->v3->co[1];
								cen[a][2]= efa->v3->co[2];
								a++;													
							}
						}
						
						if(a==2){
							glBegin(GL_LINES);
							
							glVertex3fv(cen[0]);
							glVertex3fv(cen[1]);	
												
							glEnd();
						}	
					}
				}/* end preview line drawing */			
				else{
					glColor3ub(0,128,255);
					if(efa->f1 & 8)
					{
						float cen[2][3];
						int a=0;					
						
						efa->v1->f &= ~8; // xxx
						efa->v2->f &= ~8;
						efa->v3->f &= ~8;
						if(efa->v4)efa->v4->f &= ~8;
						
						if(efa->e1->f2 & 8){							
							float nlen,npct;
							
							nlen = sqrt((efa->e1->v1->co[0] - efa->e1->v2->co[0])*(efa->e1->v1->co[0] - efa->e1->v2->co[0])+
										(efa->e1->v1->co[1] - efa->e1->v2->co[1])*(efa->e1->v1->co[1] - efa->e1->v2->co[1])+
										(efa->e1->v1->co[2] - efa->e1->v2->co[2])*(efa->e1->v1->co[2] - efa->e1->v2->co[2]));
							npct = (percentcut*slen)/nlen;
							if(npct >= 1) npct = 1;
							if(efa->e1->f2 & 32)	npct = 1-npct;

							cen[a][0]= efa->e1->v1->co[0] - ((efa->e1->v1->co[0] - efa->e1->v2->co[0]) * (npct));
							cen[a][1]= efa->e1->v1->co[1] - ((efa->e1->v1->co[1] - efa->e1->v2->co[1]) * (npct));
							cen[a][2]= efa->e1->v1->co[2] - ((efa->e1->v1->co[2] - efa->e1->v2->co[2]) * (npct));

							efa->e1->f1 = 32768*(npct);
							efa->e1->v1->f |= 8; // xxx
							efa->e1->v2->f |= 8;
							a++;
						}
						if((efa->e2->f2 & 8) && a!=2)
						{
							float nlen,npct;
							
							nlen = sqrt((efa->e2->v1->co[0] - efa->e2->v2->co[0])*(efa->e2->v1->co[0] - efa->e2->v2->co[0])+
										(efa->e2->v1->co[1] - efa->e2->v2->co[1])*(efa->e2->v1->co[1] - efa->e2->v2->co[1])+
										(efa->e2->v1->co[2] - efa->e2->v2->co[2])*(efa->e2->v1->co[2] - efa->e2->v2->co[2]));
							npct = (percentcut*slen)/nlen;
							if(npct >= 1) npct = 1;
							if(efa->e2->f2 & 32)	npct = 1-npct;

							cen[a][0]= efa->e2->v1->co[0] - ((efa->e2->v1->co[0] - efa->e2->v2->co[0]) * (npct));
							cen[a][1]= efa->e2->v1->co[1] - ((efa->e2->v1->co[1] - efa->e2->v2->co[1]) * (npct));
							cen[a][2]= efa->e2->v1->co[2] - ((efa->e2->v1->co[2] - efa->e2->v2->co[2]) * (npct));

							efa->e2->f1 = 32768*(npct);								
							efa->e2->v1->f |= 8; // xxx
							efa->e2->v2->f |= 8;
							a++;
						}
						if((efa->e3->f2 & 8) && a!=2){
							float nlen,npct;
							
							nlen = sqrt((efa->e3->v1->co[0] - efa->e3->v2->co[0])*(efa->e3->v1->co[0] - efa->e3->v2->co[0])+
										(efa->e3->v1->co[1] - efa->e3->v2->co[1])*(efa->e3->v1->co[1] - efa->e3->v2->co[1])+
										(efa->e3->v1->co[2] - efa->e3->v2->co[2])*(efa->e3->v1->co[2] - efa->e3->v2->co[2]));
							npct = (percentcut*slen)/nlen;
							if(npct >= 1) npct = 1;
							if(efa->e3->f2 & 32)	npct = 1-npct;

							cen[a][0]= efa->e3->v1->co[0] - ((efa->e3->v1->co[0] - efa->e3->v2->co[0]) * (npct));
							cen[a][1]= efa->e3->v1->co[1] - ((efa->e3->v1->co[1] - efa->e3->v2->co[1]) * (npct));
							cen[a][2]= efa->e3->v1->co[2] - ((efa->e3->v1->co[2] - efa->e3->v2->co[2]) * (npct));

							efa->e3->f1 = 32768*(npct);								
							efa->e3->v1->f |= 8; // xxx
							efa->e3->v2->f |= 8;
							a++;
						}
							
						if(efa->e4){
							if((efa->e4->f2 & 8) && a!=2){
								float nlen,npct;
								
								nlen = sqrt((efa->e4->v1->co[0] - efa->e4->v2->co[0])*(efa->e4->v1->co[0] - efa->e4->v2->co[0])+
											(efa->e4->v1->co[1] - efa->e4->v2->co[1])*(efa->e4->v1->co[1] - efa->e4->v2->co[1])+
											(efa->e4->v1->co[2] - efa->e4->v2->co[2])*(efa->e4->v1->co[2] - efa->e4->v2->co[2]));
							npct = (percentcut*slen)/nlen;
							if(npct >= 1) npct = 1;
							if(efa->e4->f2 & 32)	npct = 1-npct;

								cen[a][0]= efa->e4->v1->co[0] - ((efa->e4->v1->co[0] - efa->e4->v2->co[0]) * (npct));
								cen[a][1]= efa->e4->v1->co[1] - ((efa->e4->v1->co[1] - efa->e4->v2->co[1]) * (npct));
								cen[a][2]= efa->e4->v1->co[2] - ((efa->e4->v1->co[2] - efa->e4->v2->co[2]) * (npct));

								efa->e4->f1 = 32768*(npct);									
								efa->e4->v1->f |= 8; // xxx
								efa->e4->v2->f |= 8;
								a++;
							}
						}
						else {	/* if it's a triangular face, set the remaining vertex as the cutcurve coordinate */
							if(!(efa->v1->f & 8) && efa->v1->h==0){ // xxx
								cen[a][0]= efa->v1->co[0];
								cen[a][1]= efa->v1->co[1];
								cen[a][2]= efa->v1->co[2];
								a++;								
							}
							else if(!(efa->v2->f & 8) && efa->v2->h==0){ // xxx
								cen[a][0]= efa->v2->co[0];
								cen[a][1]= efa->v2->co[1];
								cen[a][2]= efa->v2->co[2];
								a++;								
							}
							else if(!(efa->v3->f & 8) && efa->v3->h==0){ // xxx
								cen[a][0]= efa->v3->co[0];
								cen[a][1]= efa->v3->co[1];
								cen[a][2]= efa->v3->co[2];
								a++;													
							}
						}
						
						if(a==2){
							glBegin(GL_LINES);
							
							glVertex3fv(cen[0]);
							glVertex3fv(cen[1]);	
												
							glEnd();
						}	
					}
				}
			}
			/* restore matrix transform */
	
			glPopMatrix();

			/*--------- END Preview Lines------------*/
			while(qtest()) 
			{
				unsigned short val=0;			
				event= extern_qread(&val);	/* extern_qread stores important events for the mainloop to handle */
				/* val==0 on key-release event */
		
				if(val && (event==SKEY))
				{
					if(smooth)smooth = 0;
					else smooth = 1;
				}

				if(val && (event==PKEY))
				{
					if(inset)inset = 0;
					else inset = 1;
				}

				if(val && (event==FKEY))
				{
						int ct;
						for(ct = 0; ct < ect; ct++){
							if(tagged[ct]->f2 & 32) 
								tagged[ct]->f2 &= ~32;
							else
								tagged[ct]->f2 |= 32;
						}
				}

				if(val && (event == MIDDLEMOUSE))
				{
					cut = 2;
					searching=0;
				}
				else if(val && (event==LEFTMOUSE || event==RETKEY))
				{
					searching=0;
				}

				if(val && (event==ESCKEY || event==RIGHTMOUSE ))
				{
					searching=0;
					cut = 0;
				}
				
			}			
			

			/* Determine the % on wich the loop should be cut */
			getmouseco_areawin(mval);			
			v1[0]=(float)mval[0];
			v1[1]=(float)mval[1];
			
			v2[0]=(float)start->v1->xs;
			v2[1]=(float)start->v1->ys;
			
			v3[0]=(float)start->v2->xs;
			v3[1]=(float)start->v2->ys;
			
			rc[0]= v3[0]-v2[0];
			rc[1]= v3[1]-v2[1];
			len= rc[0]*rc[0]+ rc[1]*rc[1];
				
			labda= ( rc[0]*(v1[0]-v2[0]) + rc[1]*(v1[1]-v2[1]) )/len;


			if(labda<=0.0) labda=0.0;
			else if(labda>=1.0)labda=1.0;
						
			percentcut=labda;		
			
			if(start->f2 & 32)
				percentcut = 1.0-percentcut;

		if(cut == 2){
			percentcut = 0.5;
		}

		if (G.qual & LR_SHIFTKEY){

			percentcut = (int)(percentcut*100.0)/100.0;	
		}
		else if (G.qual & LR_CTRLKEY)
			percentcut = (int)(percentcut*10.0)/10.0;		
	
		outcut = (percentcut*100.0);

		/* Build the Header Line */ 

		if(inset)
			sprintf(mesg,"Cut: %0.2f%% ",slen*percentcut);
		else
			sprintf(mesg,"Cut: %0.2f%% ",outcut);
		

		if(smooth)
			sprintf(mesg,"%s| (f)lip side | (s)mooth on  |",mesg);
		else
			sprintf(mesg,"%s| (f)lip side | (s)mooth off |",mesg);

		if(inset)
			sprintf(mesg,"%s (p)roportional on ",mesg);
		else
			sprintf(mesg,"%s (p)roportional off",mesg);
		
		headerprint(mesg);

		screen_swapbuffers();		
	}			
	
	if(cut){
		/* Now that we have selected a cut %, mark the edges for cutting. */
		if(!inset){

			for(eed = em->edges.first; eed; eed=eed->next){
				if(percentcut == 1.0)
					percentcut = 0.9999;
				else if(percentcut == 0.0)
					percentcut = 0.0001;
				if(eed->f2 & 8){
					if(eed->f2 & 32)/* Need to offset by a const. (0.5/32768) for consistant roundoff */
						eed->f1 = 32768*(1.0-percentcut - 0.0000153);
					else
						eed->f1 = 32768*(percentcut + 0.0000153);
				}
			}
		}
	/*-------------------------------------*/

			if(smooth)
				subdivideflag(8, 0, B_KNIFE | B_PERCENTSUBD | B_SMOOTH); /* B_KNIFE tells subdivide that edgeflags are already set */
			else
				subdivideflag(8, 0, B_KNIFE | B_PERCENTSUBD); /* B_KNIFE tells subdivide that edgeflags are already set */
			
			for(eed = em->edges.first; eed; eed=eed->next){							
				if(eed->v1->f & 16) eed->v1->f |= SELECT; //
				else eed->v1->f &= ~SELECT;
				
				if(eed->v2->f & 16) eed->v2->f |= SELECT;
				else eed->v2->f &= ~SELECT;
				
				/* proper edge select state, needed because subdivide still doesnt do it OK */
				if(eed->v1->f & eed->v2->f & SELECT) eed->f |= SELECT;
				else eed->f &= ~SELECT;
			}			
		}
	}
	/*----------END Cut Loop-----------------------------*/

	

	/* Clear flags */		
	for(eed = em->edges.first; eed; eed=eed->next){	
		eed->f2 &= ~(2|4|8|32|64);
		eed->v1->f &= ~(2|16); // xxx
		eed->v2->f &= ~(2|16);		
	}
	
	for(efa= em->faces.first; efa; efa=efa->next){
		efa->f1 &= ~(4|8);

		/* proper face select state, needed because subdivide still doesnt do it OK */
		if( faceselectedAND(efa, SELECT) ) efa->f |= SELECT;
		else efa->f &= ~SELECT;
	}
	
	// flushes vertex -> edge -> face selection
	EM_select_flush();
	
	countall();

	if(tagged)
		MEM_freeN(tagged);
	if(taggedsrch)
		MEM_freeN(taggedsrch);
	if(percentfacesloop)
		MEM_freeN(percentfacesloop);
	
	/* send event to redraw this window, does header too */	
	SetBlenderCursor(SYSCURSOR);
	addqueue(curarea->win, REDRAW, 1); 

	/* should have check for cancelled (ton) */
	if(mode==LOOP_CUT) BIF_undo_push("Face Loop Subdivide");
	else if(mode==LOOP_SELECT) BIF_undo_push("Select Face Loop");	

}

/* ****************************** END LOOPOPERATIONS ********************** */

void LoopMenu(){ /* Called by KKey */

	short ret;
	
	ret=pupmenu("Loop/Cut Menu %t|Face Loop Select %x1|Face Loop Cut %x2|"
				"Knife (Exact) %x3|Knife (Midpoints)%x4|");
				
	switch (ret){
		case 1:
			loopoperations(LOOP_SELECT);
			break;
		case 2:
			loopoperations(LOOP_CUT);
			break;
		case 3: 
			KnifeSubdivide(KNIFE_EXACT);
			break;
		case 4:
			KnifeSubdivide(KNIFE_MIDPOINT);
	}

}

