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
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
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
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_ghash.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"

#include "PIL_time.h"

#include "BIF_gl.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "mesh_intern.h"

/* **** XXX ******** */
static void BIF_undo_push() {}
static void BIF_undo() {}
static void error() {}
static int qtest() {return 0;}
/* **** XXX ******** */


/* New LoopCut */
static void edgering_sel(EditMesh *em, EditEdge *startedge, int select, int previewlines)
{
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
// XXX			persp(PERSP_VIEW);
// XXX			glPushMatrix();
// XXX			mymultmatrix(obedit->obmat);

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

void CutEdgeloop(Object *obedit, wmOperator *op, EditMesh *em, int numcuts)
{
	ViewContext vc; // XXX
	EditEdge *nearest=NULL, *eed;
	float fac;
	int keys = 0, holdnum=0, selectmode, dist;
	short mvalo[2] = {0, 0}, mval[2] = {0, 0};
	short event=0, val, choosing=1, cancel=0, cuthalf = 0, smooth=0;
	short hasHidden = 0;
	char msg[128];
	
	selectmode = em->selectmode;
		
	if(em->selectmode & SCE_SELECT_FACE){
		em->selectmode =  SCE_SELECT_EDGE;
		EM_selectmode_set(em);	  
	}
	
	
	BIF_undo_push("Loopcut Begin");
	while(choosing && !cancel){
// XXX		getmouseco_areawin(mval);
		if (mval[0] != mvalo[0] || mval[1] != mvalo[1]) {
			mvalo[0] = mval[0];
			mvalo[1] = mval[1];
			dist= 50;
			nearest = findnearestedge(&vc, &dist);	// returns actual distance in dist
//			scrarea_do_windraw(curarea);	// after findnearestedge, backbuf!
			
			sprintf(msg,"Number of Cuts: %d",numcuts);
			if(smooth){
				sprintf(msg,"%s (S)mooth: on",msg);
			} else {
				sprintf(msg,"%s (S)mooth: off",msg);
			}
			
//			headerprint(msg);
			/* Need to figure preview */
			if(nearest){
				edgering_sel(em, nearest, 0, numcuts);
			 }   
// XXX			screen_swapbuffers();
			
		/* backbuffer refresh for non-apples (no aux) */
#ifndef __APPLE__
// XXX			if(G.vd->drawtype>OB_WIRE && (G.vd->flag & V3D_ZBUF_SELECT)) {
//				backdrawview3d(0);
//			}
#endif
		}
		else PIL_sleep_ms(10);	// idle
		
		
		while(qtest()) 
		{
			val=0;
// XXX			event= extern_qread(&val);
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

	if(cancel){
		return;   
	}
	/* clean selection */
	for(eed=em->edges.first; eed; eed = eed->next){
		EM_select_edge(eed,0);
	}
	/* select edge ring */
	edgering_sel(em, nearest, 1, 0);
	
	/* now cut the loops */
	if(smooth){
		fac= 1.0f;
// XXX		if(fbutton(&fac, 0.0f, 5.0f, 10, 10, "Smooth:")==0) return;
		fac= 0.292f*fac;			
		esubdivideflag(obedit, em, SELECT,fac,0,B_SMOOTH,numcuts,SUBDIV_SELECT_LOOPCUT);
	} else {
		esubdivideflag(obedit, em, SELECT,0,0,0,numcuts,SUBDIV_SELECT_LOOPCUT);
	}
	/* if this was a single cut, enter edgeslide mode */
	if(numcuts == 1 && hasHidden == 0){
		if(cuthalf)
			EdgeSlide(em, op, 1,0.0);
		else {
			if(EdgeSlide(em, op, 0,0.0) == -1){
				BIF_undo();
			}
		}
	}
	
	if(em->selectmode !=  selectmode){
		em->selectmode = selectmode;
		EM_selectmode_set(em);
	}	
	
//	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	return;
}


/* *************** LOOP SELECT ************* */
#if 0
static short edgeFaces(EditMesh *em, EditEdge *e)
{
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


/* ******************************************************************** */
/* Knife Subdivide Tool.  Subdivides edges intersected by a mouse trail
	drawn by user.
	
	Currently mapped to KKey when in MeshEdit mode.
	Usage:
		Hit Shift K, Select Centers or Exact
		Hold LMB down to draw path, hit RETKEY.
		ESC cancels as expected.
   
	Contributed by Robert Wenzlaff (Det. Thorn).

    2.5 revamp:
    - non modal (no menu before cutting)
    - exit on mouse release
    - polygon/segment drawing can become handled by WM cb later

*/

#define KNIFE_EXACT		1
#define KNIFE_MIDPOINT	2
#define KNIFE_MULTICUT	3

static EnumPropertyItem knife_items[]= {
	{KNIFE_EXACT, "EXACT", 0, "Exact", ""},
	{KNIFE_MIDPOINT, "MIDPOINTS", 0, "Midpoints", ""},
	{KNIFE_MULTICUT, "MULTICUT", 0, "Multicut", ""},
	{0, NULL, 0, NULL, NULL}
};

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
	
	threshold = 0.000001; /*tolerance for vertex intersection*/
	// XXX	threshold = scene->toolsettings->select_thresh / 100;
	
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


#define MAX_CUTS 256

static int knife_cut_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	EditMesh *em= BKE_mesh_get_editmesh(((Mesh *)obedit->data));
	ARegion *ar= CTX_wm_region(C);
	EditEdge *eed;
	EditVert *eve;
	CutCurve curve[MAX_CUTS];
	struct GHash *gh;
	float isect=0.0;
	float  *scr, co[4];
	int len=0;
	short numcuts=1, mode= RNA_int_get(op->ptr, "type");
	
	/* edit-object needed for matrix, and ar->regiondata for projections to work */
	if (ELEM3(NULL, obedit, ar, ar->regiondata))
		return OPERATOR_CANCELLED;
	
	if (EM_nvertices_selected(em) < 2) {
		error("No edges are selected to operate on");
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;;
	}

	/* get the cut curve */
	RNA_BEGIN(op->ptr, itemptr, "path") {
		
		RNA_float_get_array(&itemptr, "loc", (float *)&curve[len]);
		len++;
		if(len>= MAX_CUTS) break;
	}
	RNA_END;
	
	if(len<2) {
		BKE_mesh_end_editmesh(obedit->data, em);
		return OPERATOR_CANCELLED;
	}

	/*store percentage of edge cut for KNIFE_EXACT here.*/
	for(eed=em->edges.first; eed; eed= eed->next) 
		eed->tmp.fp = 0.0; 
	
	/*the floating point coordinates of verts in screen space will be stored in a hash table according to the vertices pointer*/
	gh = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	for(eve=em->verts.first; eve; eve=eve->next){
		scr = MEM_mallocN(sizeof(float)*2, "Vertex Screen Coordinates");
		VECCOPY(co, eve->co);
		co[3]= 1.0;
		mul_m4_v4(obedit->obmat, co);
		project_float(ar, co, scr);
		BLI_ghash_insert(gh, eve, scr);
		eve->f1 = 0; /*store vertex intersection flag here*/
	
	}
	
	eed= em->edges.first;		
	while(eed) {	
		if( eed->v1->f & eed->v2->f & SELECT ){		// NOTE: uses vertex select, subdiv doesnt do edges yet
			isect= seg_intersect(eed, curve, len, mode, gh);
			if (isect!=0.0f) eed->f2= 1;
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
	
	if (mode==KNIFE_MIDPOINT) esubdivideflag(obedit, em, SELECT, 0, 0, B_KNIFE, 1, SUBDIV_SELECT_ORIG);
	else if (mode==KNIFE_MULTICUT) esubdivideflag(obedit, em, SELECT, 0, 0, B_KNIFE, numcuts, SUBDIV_SELECT_ORIG);
	else esubdivideflag(obedit, em, SELECT, 0, 0, B_KNIFE|B_PERCENTSUBD, 1, SUBDIV_SELECT_ORIG);

	eed=em->edges.first;
	while(eed){
		eed->f2=0;
		eed->f1=0;
		eed=eed->next;
	}	
	
	BLI_ghash_free(gh, NULL, (GHashValFreeFP)MEM_freeN);
	
	BKE_mesh_end_editmesh(obedit->data, em);

	DAG_id_flush_update(obedit->data, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, obedit->data);

	return OPERATOR_FINISHED;
}


void MESH_OT_knife_cut(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	ot->name= "Knife Cut";
	ot->description= "Cut selected edges and faces into parts.";
	ot->idname= "MESH_OT_knife_cut";
	
	ot->invoke= WM_gesture_lines_invoke;
	ot->modal= WM_gesture_lines_modal;
	ot->exec= knife_cut_exec;
	
	ot->poll= EM_view3d_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", knife_items, KNIFE_EXACT, "Type", "");
	prop= RNA_def_property(ot->srna, "path", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_runtime(prop, &RNA_OperatorMousePath);
	
	/* internal */
	RNA_def_int(ot->srna, "cursor", BC_KNIFECURSOR, 0, INT_MAX, "Cursor", "", 0, INT_MAX);
}

/* ******************************************************* */

