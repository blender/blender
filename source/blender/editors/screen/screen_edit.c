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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_vec_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_area.h"
#include "ED_screen.h"
#include "ED_screen_types.h"

#include "wm_subwindow.h"

#include "screen_intern.h"	/* own module include */

/* ******************* gesture manager ******************* */
void ed_gesture_draw_rect(wmWindow *win, wmGesture *gt)
{
	wmGestureRect *rect= (wmGestureRect *)gt;
	sdrawbox(rect->x1, rect->y1, rect->x2, rect->y2);
}

void ed_gesture_update(wmWindow *win)
{
	wmGesture *gt= (wmGesture *)win->gesture.first;

	while(gt) {
		if(gt->type==GESTURE_RECT)
			ed_gesture_draw_rect(win, gt);
		gt= gt->next;
	}
}

/* ******************* screen vert, edge, area managing *********************** */

static ScrVert *screen_addvert(bScreen *sc, short x, short y)
{
	ScrVert *sv= MEM_callocN(sizeof(ScrVert), "addscrvert");
	sv->vec.x= x;
	sv->vec.y= y;
	
	BLI_addtail(&sc->vertbase, sv);
	return sv;
}

static void sortscrvert(ScrVert **v1, ScrVert **v2)
{
	ScrVert *tmp;
	
	if (*v1 > *v2) {
		tmp= *v1;
		*v1= *v2;
		*v2= tmp;	
	}
}

static ScrEdge *screen_addedge(bScreen *sc, ScrVert *v1, ScrVert *v2)
{
	ScrEdge *se= MEM_callocN(sizeof(ScrEdge), "addscredge");
	
	sortscrvert(&v1, &v2);
	se->v1= v1;
	se->v2= v2;
	
	BLI_addtail(&sc->edgebase, se);
	return se;
}


static ScrEdge *screen_findedge(bScreen *sc, ScrVert *v1, ScrVert *v2)
{
	ScrEdge *se;
	
	sortscrvert(&v1, &v2);
	for (se= sc->edgebase.first; se; se= se->next)
		if(se->v1==v1 && se->v2==v2)
			return se;
	
	return NULL;
}

static ScrArea *screen_areahascursor(bScreen *scr, int x, int y)
{
	ScrArea *sa= NULL;
	sa= scr->areabase.first;
	while(sa) {
		if(BLI_in_rcti(&sa->totrct, x, y)) break;
		sa= sa->next;
	}

	return sa;
}

/* *************************** action zone operator ************************** */

/* operator state vars used:  
		none

functions:

	apply() set actionzone event

	exit()	free customdata

callbacks:

	exec()	never used

	invoke() check if in zone  
			add customdata, put mouseco and area in it
			add modal handler

	modal()	accept modal events while doing it
			call apply() with gesture info, active window, nonactive window
			call exit() and remove handler when LMB confirm

*/

typedef struct sActionzoneData {
	ScrArea *sa1, *sa2;
	AZone *az;
	int x, y, gesture_dir;
} sActionzoneData;


static AZone *is_in_area_actionzone(ScrArea *sa, int x, int y)
{
	AZone *az= NULL;
	int i= 0;
	
	for(az= sa->actionzones.first, i= 0; az; az= az->next, i++) {
		if(az && az->type == AZONE_TRI) {
			if(IsPointInTri2DInts(az->x1, az->y1, az->x2, az->y2, x, y)) 
				break;
		}
		if(az->type == AZONE_QUAD) {
			if(az->x1 < x && x < az->x2 && az->y1 < y && y < az->y2) 
				break;
		}
	}
	
	return az;
}

static int actionzone_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	AZone *az= is_in_area_actionzone(C->area, event->x, event->y);
	sActionzoneData *sad;
	
	/* quick escape */
	if(az==NULL)
		return OPERATOR_PASS_THROUGH;
	
	/* ok we do the actionzone */
	sad= op->customdata= MEM_callocN(sizeof(sActionzoneData), "sActionzoneData");
	sad->sa1= C->area;
	sad->az= az;
	sad->x= event->x; sad->y= event->y;
	
	/* add modal handler */
	WM_event_add_modal_handler(&C->window->handlers, op);
	
	return OPERATOR_RUNNING_MODAL;
}

static void actionzone_exit(bContext *C, wmOperator *op)
{
	if(op->customdata)
		MEM_freeN(op->customdata);
	op->customdata= NULL;
}

/* send EVT_ACTIONZONE event */
static void actionzone_apply(bContext *C, wmOperator *op)
{
	wmEvent event;
	
	event= *(C->window->eventstate);	/* XXX huh huh? make api call */
	event.type= EVT_ACTIONZONE;
	event.customdata= op->customdata;
	event.customdatafree= TRUE;
	op->customdata= NULL;
	
	wm_event_add(C->window, &event);
}

static int actionzone_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	sActionzoneData *sad= op->customdata;
	int deltax, deltay;

	switch(event->type) {
		case MOUSEMOVE:
			/* calculate gesture direction */
			deltax= (event->x - sad->x);
			deltay= (event->y - sad->y);
			
			if(deltay > ABS(deltax))
				sad->gesture_dir= AZONE_N;
			else if(deltax > ABS(deltay))
				sad->gesture_dir= AZONE_E;
			else if(deltay < -ABS(deltax))
				sad->gesture_dir= AZONE_S;
			else
				sad->gesture_dir= AZONE_W;
			
			/* gesture is large enough? */
			if(ABS(deltax) > 12 || ABS(deltay) > 12) {
				
				/* second area, for join */
				sad->sa2= screen_areahascursor(C->screen, event->x, event->y);
				/* apply sends event */
				actionzone_apply(C, op);
				actionzone_exit(C, op);
				
				WM_event_remove_modal_handler(&C->window->handlers, op);
				
				return OPERATOR_FINISHED;
			}
			break;
		case ESCKEY:
		case LEFTMOUSE:
			actionzone_exit(C, op);
			WM_event_remove_modal_handler(&C->window->handlers, op);
			return OPERATOR_CANCELLED;
	}
	
	return OPERATOR_RUNNING_MODAL;
}

void ED_SCR_OT_actionzone(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Handle area action zones";
	ot->idname= "ED_SCR_OT_actionzone";
	
	ot->invoke= actionzone_invoke;
	ot->modal= actionzone_modal;
	
	ot->poll= ED_operator_areaactive;
}

/* ************************************** */

static void removedouble_scrverts(bScreen *sc)
{
	ScrVert *v1, *verg;
	ScrEdge *se;
	ScrArea *sa;
	
	verg= sc->vertbase.first;
	while(verg) {
		if(verg->newv==NULL) {	/* !!! */
			v1= verg->next;
			while(v1) {
				if(v1->newv==NULL) {	/* !?! */
					if(v1->vec.x==verg->vec.x && v1->vec.y==verg->vec.y) {
						/* printf("doublevert\n"); */
						v1->newv= verg;
					}
				}
				v1= v1->next;
			}
		}
		verg= verg->next;
	}

	/* replace pointers in edges and faces */
	se= sc->edgebase.first;
	while(se) {
		if(se->v1->newv) se->v1= se->v1->newv;
		if(se->v2->newv) se->v2= se->v2->newv;
		/* edges changed: so.... */
		sortscrvert(&(se->v1), &(se->v2));
		se= se->next;
	}
	sa= sc->areabase.first;
	while(sa) {
		if(sa->v1->newv) sa->v1= sa->v1->newv;
		if(sa->v2->newv) sa->v2= sa->v2->newv;
		if(sa->v3->newv) sa->v3= sa->v3->newv;
		if(sa->v4->newv) sa->v4= sa->v4->newv;
		sa= sa->next;
	}

	/* remove */
	verg= sc->vertbase.first;
	while(verg) {
		v1= verg->next;
		if(verg->newv) {
			BLI_remlink(&sc->vertbase, verg);
			MEM_freeN(verg);
		}
		verg= v1;
	}

}

static void removenotused_scrverts(bScreen *sc)
{
	ScrVert *sv, *svn;
	ScrEdge *se;
	
	/* we assume edges are ok */
	
	se= sc->edgebase.first;
	while(se) {
		se->v1->flag= 1;
		se->v2->flag= 1;
		se= se->next;
	}
	
	sv= sc->vertbase.first;
	while(sv) {
		svn= sv->next;
		if(sv->flag==0) {
			BLI_remlink(&sc->vertbase, sv);
			MEM_freeN(sv);
		}
		else sv->flag= 0;
		sv= svn;
	}
}

static void removedouble_scredges(bScreen *sc)
{
	ScrEdge *verg, *se, *sn;
	
	/* compare */
	verg= sc->edgebase.first;
	while(verg) {
		se= verg->next;
		while(se) {
			sn= se->next;
			if(verg->v1==se->v1 && verg->v2==se->v2) {
				BLI_remlink(&sc->edgebase, se);
				MEM_freeN(se);
			}
			se= sn;
		}
		verg= verg->next;
	}
}

static void removenotused_scredges(bScreen *sc)
{
	ScrEdge *se, *sen;
	ScrArea *sa;
	int a=0;
	
	/* sets flags when edge is used in area */
	sa= sc->areabase.first;
	while(sa) {
		se= screen_findedge(sc, sa->v1, sa->v2);
		if(se==0) printf("error: area %d edge 1 doesn't exist\n", a);
		else se->flag= 1;
		se= screen_findedge(sc, sa->v2, sa->v3);
		if(se==0) printf("error: area %d edge 2 doesn't exist\n", a);
		else se->flag= 1;
		se= screen_findedge(sc, sa->v3, sa->v4);
		if(se==0) printf("error: area %d edge 3 doesn't exist\n", a);
		else se->flag= 1;
		se= screen_findedge(sc, sa->v4, sa->v1);
		if(se==0) printf("error: area %d edge 4 doesn't exist\n", a);
		else se->flag= 1;
		sa= sa->next;
		a++;
	}
	se= sc->edgebase.first;
	while(se) {
		sen= se->next;
		if(se->flag==0) {
			BLI_remlink(&sc->edgebase, se);
			MEM_freeN(se);
		}
		else se->flag= 0;
		se= sen;
	}
}

static int scredge_is_horizontal(ScrEdge *se)
{
	return (se->v1->vec.y == se->v2->vec.y);
}

static ScrEdge *screen_find_active_scredge(bScreen *sc, int mx, int my)
{
	ScrEdge *se;
	
	for (se= sc->edgebase.first; se; se= se->next) {
		if (scredge_is_horizontal(se)) {
			short min, max;
			min= MIN2(se->v1->vec.x, se->v2->vec.x);
			max= MAX2(se->v1->vec.x, se->v2->vec.x);
			
			if (abs(my-se->v1->vec.y)<=2 && mx>=min && mx<=max)
				return se;
		} 
		else {
			short min, max;
			min= MIN2(se->v1->vec.y, se->v2->vec.y);
			max= MAX2(se->v1->vec.y, se->v2->vec.y);
			
			if (abs(mx-se->v1->vec.x)<=2 && my>=min && my<=max)
				return se;
		}
	}
	
	return NULL;
}

/* danger: is used while areamove! */
static void select_connected_scredge(bScreen *sc, ScrEdge *edge)
{
	ScrEdge *se;
	ScrVert *sv;
	int oneselected;
	char dir;
	
	/* select connected, only in the right direction */
	/* 'dir' is the direction of EDGE */
	
	if(edge->v1->vec.x==edge->v2->vec.x) dir= 'v';
	else dir= 'h';
	
	sv= sc->vertbase.first;
	while(sv) {
		sv->flag = 0;
		sv= sv->next;
	}
	
	edge->v1->flag= 1;
	edge->v2->flag= 1;
	
	oneselected= 1;
	while(oneselected) {
		se= sc->edgebase.first;
		oneselected= 0;
		while(se) {
			if(se->v1->flag + se->v2->flag==1) {
				if(dir=='h') if(se->v1->vec.y==se->v2->vec.y) {
					se->v1->flag= se->v2->flag= 1;
					oneselected= 1;
				}
				if(dir=='v') if(se->v1->vec.x==se->v2->vec.x) {
					se->v1->flag= se->v2->flag= 1;
					oneselected= 1;
				}
			}
			se= se->next;
		}
	}
}

static ScrArea *screen_addarea(bScreen *sc, ScrVert *v1, ScrVert *v2, ScrVert *v3, ScrVert *v4, short headertype, short spacetype)
{
	ScrArea *sa= MEM_callocN(sizeof(ScrArea), "addscrarea");
	sa->v1= v1;
	sa->v2= v2;
	sa->v3= v3;
	sa->v4= v4;
	sa->headertype= headertype;
	sa->spacetype= spacetype;
	
	BLI_addtail(&sc->areabase, sa);
	
	return sa;
}

static void screen_delarea(bScreen *sc, ScrArea *sa)
{
	/* XXX need context to cancel operators ED_area_exit(C, sa); */
	BKE_screen_area_free(sa);
	BLI_remlink(&sc->areabase, sa);
	MEM_freeN(sa);
}

/* Helper function to join 2 areas, it has a return value, 0=failed 1=success
 * 	used by the split, join and rip operators
 */
int screen_join_areas(bScreen *scr, ScrArea *sa1, ScrArea *sa2);

static bScreen *addscreen_area(wmWindow *win, char *name, short headertype, short spacetype)
{
	bScreen *sc;
	ScrVert *sv1, *sv2, *sv3, *sv4;
	
	sc= alloc_libblock(&G.main->screen, ID_SCR, name);
	
	sc->scene= G.scene;
	
	sv1= screen_addvert(sc, 0, 0);
	sv2= screen_addvert(sc, 0, win->sizey-1);
	sv3= screen_addvert(sc, win->sizex-1, win->sizey-1);
	sv4= screen_addvert(sc, win->sizex-1, 0);
	
	screen_addedge(sc, sv1, sv2);
	screen_addedge(sc, sv2, sv3);
	screen_addedge(sc, sv3, sv4);
	screen_addedge(sc, sv4, sv1);
	
	screen_addarea(sc, sv1, sv2, sv3, sv4, headertype, spacetype);
		
	return sc;
}

static bScreen *addscreen(wmWindow *win, char *name) 
{
	return addscreen_area(win, name, HEADERDOWN, SPACE_INFO);
}

static void screen_copy(bScreen *to, bScreen *from)
{
	ScrVert *s1, *s2;
	ScrEdge *se;
	ScrArea *sa, *saf;
	
	/* free contents of 'to', is from blenkernel screen.c */
	free_screen(to);
	
	BLI_duplicatelist(&to->vertbase, &from->vertbase);
	BLI_duplicatelist(&to->edgebase, &from->edgebase);
	BLI_duplicatelist(&to->areabase, &from->areabase);
	to->regionbase.first= to->regionbase.last= NULL;
	
	s2= to->vertbase.first;
	for(s1= from->vertbase.first; s1; s1= s1->next, s2= s2->next) {
		s1->newv= s2;
	}
	
	for(se= to->edgebase.first; se; se= se->next) {
		se->v1= se->v1->newv;
		se->v2= se->v2->newv;
		sortscrvert(&(se->v1), &(se->v2));
	}
	
	saf= from->areabase.first;
	for(sa= to->areabase.first; sa; sa= sa->next, saf= saf->next) {
		sa->v1= sa->v1->newv;
		sa->v2= sa->v2->newv;
		sa->v3= sa->v3->newv;
		sa->v4= sa->v4->newv;
		
		sa->spacedata.first= sa->spacedata.last= NULL;
		sa->uiblocks.first= sa->uiblocks.last= NULL;
		sa->panels.first= sa->panels.last= NULL;
		sa->regionbase.first= sa->regionbase.last= NULL;
		sa->actionzones.first= sa->actionzones.last= NULL;
		sa->scriptlink.totscript= 0;
		
		area_copy_data(sa, saf, 0);
	}
	
	/* put at zero (needed?) */
	for(s1= from->vertbase.first; s1; s1= s1->next)
		s1->newv= NULL;

}

bScreen *ED_screen_riparea(struct wmWindow *win, bScreen *sc, struct ScrArea *sa)
{
	bScreen *newsc=NULL;
	ScrArea *newa;
	ScrArea *tsa;

	if(sc->full != SCREENNORMAL) return NULL; /* XXX handle this case! */
	
	/* make new screen: */
	newsc= addscreen_area(win, sc->id.name+2, sa->headertype, sa->spacetype);

	/* new area is first (and only area) added to new win */
	newa = (ScrArea *)newsc->areabase.first;
	area_copy_data(newa, sa, 0);

	/*remove the original area if possible*/
	for(tsa= sc->areabase.first; tsa; tsa= tsa->next) {
		if (screen_join_areas(sc,tsa,sa)) 
			break;
	}

	removedouble_scredges(sc);
	removenotused_scredges(sc);
	removenotused_scrverts(sc);

	return newsc;
}

bScreen *ED_screen_duplicate(wmWindow *win, bScreen *sc)
{
	bScreen *newsc;
	
	if(sc->full != SCREENNORMAL) return NULL; /* XXX handle this case! */
	
	/* make new screen: */
	newsc= addscreen(win, sc->id.name+2);
	/* copy all data */
	screen_copy(newsc, sc);
	
	return newsc;
}

/* with sa as center, sb is located at: 0=W, 1=N, 2=E, 3=S */
/* used with split operator */
static ScrEdge *area_findsharededge(bScreen *screen, ScrArea *sa, ScrArea *sb)
{
	ScrVert *sav1= sa->v1;
	ScrVert *sav2= sa->v2;
	ScrVert *sav3= sa->v3;
	ScrVert *sav4= sa->v4;
	ScrVert *sbv1= sb->v1;
	ScrVert *sbv2= sb->v2;
	ScrVert *sbv3= sb->v3;
	ScrVert *sbv4= sb->v4;
	
	if(sav1==sbv4 && sav2==sbv3) { /* sa to right of sb = W */
		return screen_findedge(screen, sav1, sav2);
	}
	else if(sav2==sbv1 && sav3==sbv4) { /* sa to bottom of sb = N */
		return screen_findedge(screen, sav2, sav3);
	}
	else if(sav3==sbv2 && sav4==sbv1) { /* sa to left of sb = E */
		return screen_findedge(screen, sav3, sav4);
	}
	else if(sav1==sbv2 && sav4==sbv3) { /* sa on top of sb = S*/
		return screen_findedge(screen, sav1, sav4);
	}
	
	return NULL;
}

/* with sa as center, sb is located at: 0=W, 1=N, 2=E, 3=S */
/* -1 = not valid check */
/* used with join operator */
static int area_getorientation(bScreen *screen, ScrArea *sa, ScrArea *sb)
{
	ScrVert *sav1, *sav2, *sav3, *sav4;
	ScrVert *sbv1, *sbv2, *sbv3, *sbv4;

	if(sa==NULL || sb==NULL) return -1;

	sav1= sa->v1;
	sav2= sa->v2;
	sav3= sa->v3;
	sav4= sa->v4;
	sbv1= sb->v1;
	sbv2= sb->v2;
	sbv3= sb->v3;
	sbv4= sb->v4;
	
	if(sav1==sbv4 && sav2==sbv3) { /* sa to right of sb = W */
		return 0;
	}
	else if(sav2==sbv1 && sav3==sbv4) { /* sa to bottom of sb = N */
		return 1;
	}
	else if(sav3==sbv2 && sav4==sbv1) { /* sa to left of sb = E */
		return 2;
	}
	else if(sav1==sbv2 && sav4==sbv3) { /* sa on top of sb = S*/
		return 3;
	}
	
	return -1;
}

/* return 0: no split possible */
/* else return (integer) screencoordinate split point */
static short testsplitpoint(wmWindow *win, ScrArea *sa, char dir, float fac)
{
	short x, y;
	
	// area big enough?
	if(sa->v4->vec.x- sa->v1->vec.x <= 2*AREAMINX) return 0;
	if(sa->v2->vec.y- sa->v1->vec.y <= 2*AREAMINY) return 0;

	// to be sure
	if(fac<0.0) fac= 0.0;
	if(fac>1.0) fac= 1.0;
	
	if(dir=='h') {
		y= sa->v1->vec.y+ fac*(sa->v2->vec.y- sa->v1->vec.y);
		
		if(sa->v2->vec.y==win->sizey-1 && sa->v2->vec.y- y < HEADERY) 
			y= sa->v2->vec.y- HEADERY;

		else if(sa->v1->vec.y==0 && y- sa->v1->vec.y < HEADERY)
			y= sa->v1->vec.y+ HEADERY;

		else if(y- sa->v1->vec.y < AREAMINY) y= sa->v1->vec.y+ AREAMINY;
		else if(sa->v2->vec.y- y < AREAMINY) y= sa->v2->vec.y- AREAMINY;
		else y-= (y % AREAGRID);

		return y;
	}
	else {
		x= sa->v1->vec.x+ fac*(sa->v4->vec.x- sa->v1->vec.x);
		if(x- sa->v1->vec.x < AREAMINX) x= sa->v1->vec.x+ AREAMINX;
		else if(sa->v4->vec.x- x < AREAMINX) x= sa->v4->vec.x- AREAMINX;
		else x-= (x % AREAGRID);

		return x;
	}
}

static ScrArea *splitarea(wmWindow *win, bScreen *sc, ScrArea *sa, char dir, float fac)
{
	ScrArea *newa=NULL;
	ScrVert *sv1, *sv2;
	short split;
	
	if(sa==0) return NULL;
	
	split= testsplitpoint(win, sa, dir, fac);
	if(split==0) return NULL;
	
	//sc= G.curscreen;
	
	//areawinset(sa->win);
	
	if(dir=='h') {
		/* new vertices */
		sv1= screen_addvert(sc, sa->v1->vec.x, split);
		sv2= screen_addvert(sc, sa->v4->vec.x, split);
		
		/* new edges */
		screen_addedge(sc, sa->v1, sv1);
		screen_addedge(sc, sv1, sa->v2);
		screen_addedge(sc, sa->v3, sv2);
		screen_addedge(sc, sv2, sa->v4);
		screen_addedge(sc, sv1, sv2);
		
		/* new areas: top */
		newa= screen_addarea(sc, sv1, sa->v2, sa->v3, sv2, sa->headertype, sa->spacetype);
		area_copy_data(newa, sa, 0);

		/* area below */
		sa->v2= sv1;
		sa->v3= sv2;
		
	}
	else {
		/* new vertices */
		sv1= screen_addvert(sc, split, sa->v1->vec.y);
		sv2= screen_addvert(sc, split, sa->v2->vec.y);
		
		/* new edges */
		screen_addedge(sc, sa->v1, sv1);
		screen_addedge(sc, sv1, sa->v4);
		screen_addedge(sc, sa->v2, sv2);
		screen_addedge(sc, sv2, sa->v3);
		screen_addedge(sc, sv1, sv2);
		
		/* new areas: left */
		newa= screen_addarea(sc, sa->v1, sa->v2, sv2, sv1, sa->headertype, sa->spacetype);
		area_copy_data(newa, sa, 0);

		/* area right */
		sa->v1= sv1;
		sa->v2= sv2;
	}
	
	/* remove double vertices en edges */
	removedouble_scrverts(sc);
	removedouble_scredges(sc);
	removenotused_scredges(sc);
	
	return newa;
}


/* Helper function to join 2 areas, it has a return value, 0=failed 1=success
 * 	used by the split, join and rip operators
 */
int screen_join_areas(bScreen* scr, ScrArea *sa1, ScrArea *sa2) 
{
	int dir;
	
	dir = area_getorientation(scr, sa1, sa2);
	/*printf("dir is : %i \n", dir);*/
	
	if (dir < 0)
	{
		if (sa1 ) sa1->flag &= ~AREA_FLAG_DRAWJOINFROM;
		if (sa2 ) sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
		return 0;
	}
	
	if(dir == 0) {
		sa1->v1= sa2->v1;
		sa1->v2= sa2->v2;
		screen_addedge(scr, sa1->v2, sa1->v3);
		screen_addedge(scr, sa1->v1, sa1->v4);
	}
	else if(dir == 1) {
		sa1->v2= sa2->v2;
		sa1->v3= sa2->v3;
		screen_addedge(scr, sa1->v1, sa1->v2);
		screen_addedge(scr, sa1->v3, sa1->v4);
	}
	else if(dir == 2) {
		sa1->v3= sa2->v3;
		sa1->v4= sa2->v4;
		screen_addedge(scr, sa1->v2, sa1->v3);
		screen_addedge(scr, sa1->v1, sa1->v4);
	}
	else if(dir == 3) {
		sa1->v1= sa2->v1;
		sa1->v4= sa2->v4;
		screen_addedge(scr, sa1->v1, sa1->v2);
		screen_addedge(scr, sa1->v3, sa1->v4);
	}
	
	screen_delarea(scr, sa2);
	removedouble_scrverts(scr);
	sa1->flag &= ~AREA_FLAG_DRAWJOINFROM;
	
	return 1;
}

/* *************************************************************** */

/* test if screen vertices should be scaled */
void screen_test_scale(bScreen *sc, int winsizex, int winsizey)
{
	ScrVert *sv=NULL;
	ScrArea *sa, *san;
	int sizex, sizey;
	float facx, facy, tempf, min[2], max[2];
	
	/* calculate size */
	min[0]= min[1]= 10000.0f;
	max[0]= max[1]= 0.0f;
	
	for(sv= sc->vertbase.first; sv; sv= sv->next) {
		min[0]= MIN2(min[0], sv->vec.x);
		min[1]= MIN2(min[1], sv->vec.y);
		max[0]= MAX2(max[0], sv->vec.x);
		max[1]= MAX2(max[1], sv->vec.y);
	}
	
	/* always make 0.0 left under */
	for(sv= sc->vertbase.first; sv; sv= sv->next) {
		sv->vec.x -= min[0];
		sv->vec.y -= min[1];
	}
	
	sizex= max[0]-min[0];
	sizey= max[1]-min[1];
	
	if(sizex!= winsizex || sizey!= winsizey) {
		facx= winsizex;
		facx/= (float)sizex;
		facy= winsizey;
		facy/= (float)sizey;
		
		/* make sure it fits! */
		for(sv= sc->vertbase.first; sv; sv= sv->next) {
			tempf= ((float)sv->vec.x)*facx;
			sv->vec.x= (short)(tempf+0.5);
			sv->vec.x+= AREAGRID-1;
			sv->vec.x-=  (sv->vec.x % AREAGRID); 
			
			CLAMP(sv->vec.x, 0, winsizex);
			
			tempf= ((float)sv->vec.y )*facy;
			sv->vec.y= (short)(tempf+0.5);
			sv->vec.y+= AREAGRID-1;
			sv->vec.y-=  (sv->vec.y % AREAGRID); 
			
			CLAMP(sv->vec.y, 0, winsizey);
		}
	}
	
	/* test for collapsed areas. This could happen in some blender version... */
	for(sa= sc->areabase.first; sa; sa= san) {
		san= sa->next;
		if(sa->v1==sa->v2 || sa->v3==sa->v4 || sa->v2==sa->v3)
			screen_delarea(sc, sa);
	}
}



#define SCR_BACK 0.55
#define SCR_ROUND 12

/* draw vertical shape visualising future joining (left as well
 * right direction of future joining) */
static void draw_horizontal_join_shape(ScrArea *sa, char dir)
{
	vec2f points[10];
	short i;
	float w, h;
	float width = sa->v3->vec.x - sa->v1->vec.x;
	float height = sa->v3->vec.y - sa->v1->vec.y;

	if(height<width) {
		h = height/8;
		w = height/4;
	}
	else {
		h = width/8;
		w = width/4;
	}

	points[0].x = sa->v1->vec.x;
	points[0].y = sa->v1->vec.y + height/2;

	points[1].x = sa->v1->vec.x;
	points[1].y = sa->v1->vec.y;

	points[2].x = sa->v4->vec.x - w;
	points[2].y = sa->v4->vec.y;

	points[3].x = sa->v4->vec.x - w;
	points[3].y = sa->v4->vec.y + height/2 - 2*h;

	points[4].x = sa->v4->vec.x - 2*w;
	points[4].y = sa->v4->vec.y + height/2;

	points[5].x = sa->v4->vec.x - w;
	points[5].y = sa->v4->vec.y + height/2 + 2*h;

	points[6].x = sa->v3->vec.x - w;
	points[6].y = sa->v3->vec.y;

	points[7].x = sa->v2->vec.x;
	points[7].y = sa->v2->vec.y;

	points[8].x = sa->v4->vec.x;
	points[8].y = sa->v4->vec.y + height/2 - h;

	points[9].x = sa->v4->vec.x;
	points[9].y = sa->v4->vec.y + height/2 + h;

	if(dir=='l') {
		/* when direction is left, then we flip direction of arrow */
		float cx = sa->v1->vec.x + width;
		for(i=0;i<10;i++) {
			points[i].x -= cx;
			points[i].x = -points[i].x;
			points[i].x += sa->v1->vec.x;
		}
	}

	glBegin(GL_POLYGON);
	for(i=0;i<5;i++)
		glVertex2f(points[i].x, points[i].y);
	glEnd();
	glBegin(GL_POLYGON);
	for(i=4;i<8;i++)
		glVertex2f(points[i].x, points[i].y);
	glVertex2f(points[0].x, points[0].y);
	glEnd();

	glRectf(points[2].x, points[2].y, points[8].x, points[8].y);
	glRectf(points[6].x, points[6].y, points[9].x, points[9].y);
}

/* draw vertical shape visualising future joining (up/down direction) */
static void draw_vertical_join_shape(ScrArea *sa, char dir)
{
	vec2f points[10];
	short i;
	float w, h;
	float width = sa->v3->vec.x - sa->v1->vec.x;
	float height = sa->v3->vec.y - sa->v1->vec.y;

	if(height<width) {
		h = height/4;
		w = height/8;
	}
	else {
		h = width/4;
		w = width/8;
	}

	points[0].x = sa->v1->vec.x + width/2;
	points[0].y = sa->v3->vec.y;

	points[1].x = sa->v2->vec.x;
	points[1].y = sa->v2->vec.y;

	points[2].x = sa->v1->vec.x;
	points[2].y = sa->v1->vec.y + h;

	points[3].x = sa->v1->vec.x + width/2 - 2*w;
	points[3].y = sa->v1->vec.y + h;

	points[4].x = sa->v1->vec.x + width/2;
	points[4].y = sa->v1->vec.y + 2*h;

	points[5].x = sa->v1->vec.x + width/2 + 2*w;
	points[5].y = sa->v1->vec.y + h;

	points[6].x = sa->v4->vec.x;
	points[6].y = sa->v4->vec.y + h;
	
	points[7].x = sa->v3->vec.x;
	points[7].y = sa->v3->vec.y;

	points[8].x = sa->v1->vec.x + width/2 - w;
	points[8].y = sa->v1->vec.y;

	points[9].x = sa->v1->vec.x + width/2 + w;
	points[9].y = sa->v1->vec.y;

	if(dir=='u') {
		/* when direction is up, then we flip direction of arrow */
		float cy = sa->v1->vec.y + height;
		for(i=0;i<10;i++) {
			points[i].y -= cy;
			points[i].y = -points[i].y;
			points[i].y += sa->v1->vec.y;
		}
	}

	glBegin(GL_POLYGON);
	for(i=0;i<5;i++)
		glVertex2f(points[i].x, points[i].y);
	glEnd();
	glBegin(GL_POLYGON);
	for(i=4;i<8;i++)
		glVertex2f(points[i].x, points[i].y);
	glVertex2f(points[0].x, points[0].y);
	glEnd();

	glRectf(points[2].x, points[2].y, points[8].x, points[8].y);
	glRectf(points[6].x, points[6].y, points[9].x, points[9].y);
}

/* draw join shape due to direction of joining */
static void draw_join_shape(ScrArea *sa, char dir)
{
	if(dir=='u' || dir=='d')
		draw_vertical_join_shape(sa, dir);
	else
		draw_horizontal_join_shape(sa, dir);
}

/* draw screen area darker with arrow (visualisation of future joining) */
static void scrarea_draw_shape_dark(ScrArea *sa, char dir)
{
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glColor4ub(0, 0, 0, 50);
	draw_join_shape(sa, dir);
	glDisable(GL_BLEND);
}

/* draw screen area ligher with arrow shape ("eraser" of previous dark shape) */
static void scrarea_draw_shape_light(ScrArea *sa, char dir)
{
	glBlendFunc(GL_DST_COLOR, GL_SRC_ALPHA);
	glEnable(GL_BLEND);
	/* value 181 was hardly computed: 181~105 */
	glColor4ub(255, 255, 255, 50);		
	/* draw_join_shape(sa, dir); */
	glRecti(sa->v1->vec.x, sa->v1->vec.y, sa->v3->vec.x, sa->v3->vec.y);
	glDisable(GL_BLEND);
}

/** screen edges drawing **/
static void drawscredge_area(ScrArea *sa)
{
	AZone *az;
	short x1= sa->v1->vec.x;
	short y1= sa->v1->vec.y;
	short x2= sa->v3->vec.x;
	short y2= sa->v3->vec.y;
	
	cpack(0x0);
	
	/* right border area */
	sdrawline(x2, y1, x2, y2);
	
	/* left border area */
	if(x1>0) { /* otherwise it draws the emboss of window over */
		sdrawline(x1, y1, x1, y2);
	}
	
	/* top border area */
	sdrawline(x1, y2, x2, y2);
	
	/* bottom border area */
	sdrawline(x1, y1, x2, y1);
	
	/* temporary viz for 'action corner' */
	for(az= sa->actionzones.first; az; az= az->next) {
		if(az->type==AZONE_TRI) sdrawtrifill(az->x1, az->y1, az->x2, az->y2, .2, .2, .2);
		//if(az->type==AZONE_TRI) sdrawtri(az->x1, az->y1, az->x2, az->y2);
	}
}

void ED_screen_do_listen(wmWindow *win, wmNotifier *note)
{
	
	/* generic notes */
	switch(note->type) {
		case WM_NOTE_WINDOW_REDRAW:
			win->screen->do_draw= 1;
			break;
		case WM_NOTE_SCREEN_CHANGED:
			win->screen->do_draw= win->screen->do_refresh= 1;
			break;
		case WM_NOTE_AREA_SPLIT:
			printf("WM_NOTE_AREA_SPLIT\n");
			break;
		case WM_NOTE_AREA_DRAG:
			printf("WM_NOTE_AREA_DRAG\n");
			break;
		case WM_NOTE_GESTURE_CHANGED:
			printf("WM_NOTE_GESTURE_CHANGED\n");
			win->screen->do_gesture= 1;
			break;
	}
}


void ED_screen_draw(wmWindow *win)
{
	ScrArea *sa;
	ScrArea *sa1=NULL;
	ScrArea *sa2=NULL;
	int dir = -1;
	int dira = -1;

	wm_subwindow_set(win, win->screen->mainwin);
	
	for(sa= win->screen->areabase.first; sa; sa= sa->next) {
		if (sa->flag & AREA_FLAG_DRAWJOINFROM) sa1 = sa;
		if (sa->flag & AREA_FLAG_DRAWJOINTO) sa2 = sa;
		drawscredge_area(sa);
	}

	if (sa1 && sa2) {
		dir = area_getorientation(win->screen, sa1, sa2);
		if (dir >= 0) {
			switch(dir) {
				case 0: /* W */
					dir = 'r';
					dira = 'l';
					break;
				case 1: /* N */
					dir = 'd';
					dira = 'u';
					break;
				case 2: /* E */
					dir = 'l';
					dira = 'r';
					break;
				case 3: /* S */
					dir = 'u';
					dira = 'd';
					break;
			}
		}
		scrarea_draw_shape_dark(sa2, dir);
		scrarea_draw_shape_light(sa1, dira);
	}
	if(G.f & G_DEBUG) printf("draw screen\n");
	win->screen->do_draw= 0;
}

void ED_screen_gesture(wmWindow *win)
{
	if(G.f & G_DEBUG) printf("gesture draw screen\n");

	if(win->gesture.first) {
		ed_gesture_update(win);
	}
	win->screen->do_gesture= 0;
}

/* make this screen usable */
/* for file read and first use, for scaling window, area moves */
void ED_screen_refresh(wmWindowManager *wm, wmWindow *win)
{
	ScrArea *sa;
	ARegion *ar;
	rcti winrct= {0, win->sizex, 0, win->sizey};
	
	screen_test_scale(win->screen, win->sizex, win->sizey);
	
	if(win->screen->mainwin==0)
		win->screen->mainwin= wm_subwindow_open(win, &winrct);
	else
		wm_subwindow_position(win, win->screen->mainwin, &winrct);
	
	for(sa= win->screen->areabase.first; sa; sa= sa->next) {
		/* set spacetype and region callbacks */
		/* sets subwindow */
		ED_area_initialize(wm, win, sa);
	}

	for(ar= win->screen->regionbase.first; ar; ar= ar->next) {
		/* set subwindow */
		ED_region_initialize(wm, win, ar);
	}
	
	if(G.f & G_DEBUG) printf("set screen\n");
	win->screen->do_refresh= 0;

}

/* file read, set all screens, ... */
void ED_screens_initialize(wmWindowManager *wm)
{
	wmWindow *win;
	
	for(win= wm->windows.first; win; win= win->next) {
		
		if(win->screen==NULL)
			win->screen= G.main->screen.first;
		
		ED_screen_refresh(wm, win);
	}
}

void ED_region_exit(bContext *C, ARegion *ar)
{
	WM_operator_cancel(C, &ar->modalops, NULL);
	WM_event_remove_handlers(&ar->handlers);
}

void ED_area_exit(bContext *C, ScrArea *sa)
{
	ARegion *ar;

	for(ar= sa->regionbase.first; ar; ar= ar->next)
		ED_region_exit(C, ar);

	WM_operator_cancel(C, &sa->modalops, NULL);
	WM_event_remove_handlers(&sa->handlers);
}

void ED_screen_exit(bContext *C, wmWindow *window, bScreen *screen)
{
	ScrArea *sa;
	ARegion *ar;

	for(ar= screen->regionbase.first; ar; ar= ar->next)
		ED_region_exit(C, ar);

	for(sa= screen->areabase.first; sa; sa= sa->next)
		ED_area_exit(C, sa);

	WM_operator_cancel(C, &window->modalops, NULL);
	WM_event_remove_handlers(&window->handlers);
}

void placeholder()
{
	removenotused_scrverts(NULL);
	removenotused_scredges(NULL);
}

/* called in wm_event_system.c. sets state var in screen */
void ED_screen_set_subwinactive(wmWindow *win)
{
	if(win->screen) {
		wmEvent *event= win->eventstate;
		ScrArea *sa;
		
		for(sa= win->screen->areabase.first; sa; sa= sa->next) {
			if(event->x > sa->totrct.xmin && event->x < sa->totrct.xmax)
				if(event->y > sa->totrct.ymin && event->y < sa->totrct.ymax)
					break;
		}
		if(sa) {
			ARegion *ar;
			for(ar= sa->regionbase.first; ar; ar= ar->next) {
				if(BLI_in_rcti(&ar->winrct, event->x, event->y))
					win->screen->subwinactive= ar->swinid;
			}
		}
		else
			win->screen->subwinactive= win->screen->mainwin;
		
	}
}

/* ****************** cursor near edge operator ********************************* */

/* operator cb */
int screen_cursor_test(bContext *C, wmOperator *op, wmEvent *event)
{
	if (C->screen->subwinactive==C->screen->mainwin) {
		ScrEdge *actedge= screen_find_active_scredge(C->screen, event->x, event->y);
		
		if (actedge && scredge_is_horizontal(actedge)) {
			WM_set_cursor(C, CURSOR_Y_MOVE);
		} else {
			WM_set_cursor(C, CURSOR_X_MOVE);
		}
	} else {
		ScrArea *sa= NULL;
		AZone *az= NULL;
		for(sa= C->screen->areabase.first; sa; sa= sa->next) {
			az= is_in_area_actionzone(sa, event->x, event->y);
			if(az!=NULL) break;
		}
		if(az!=NULL) WM_set_cursor(C, CURSOR_EDIT);
		else WM_set_cursor(C, CURSOR_STD);
	}
	
	return OPERATOR_PASS_THROUGH;
}

/* ************** move area edge operator *********************************** */

/* operator state vars used:  
           x, y   			mouse coord near edge
           delta            movement of edge

	functions:

	init()   set default property values, find edge based on mouse coords, test
            if the edge can be moved, select edges, calculate min and max movement

	apply()	apply delta on selection

	exit()	cleanup, send notifier

	cancel() cancel moving

	callbacks:

	exec()   execute without any user interaction, based on properties
            call init(), apply(), exit()

	invoke() gets called on mouse click near edge
            call init(), add handler

	modal()  accept modal events while doing it
			call apply() with delta motion
            call exit() and remove handler

*/

typedef struct sAreaMoveData {
	int bigger, smaller, origval;
	char dir;
} sAreaMoveData;

/* helper call to move area-edge, sets limits */
static void move_areas_set_limits(bScreen *sc, int dir, int *bigger, int *smaller)
{
	ScrArea *sa;
	
	/* we check all areas and test for free space with MINSIZE */
	*bigger= *smaller= 100000;
	
	for(sa= sc->areabase.first; sa; sa= sa->next) {
		if(dir=='h') {
			int y1= sa->v2->vec.y - sa->v1->vec.y-AREAMINY;
			
			/* if top or down edge selected, test height */
			if(sa->v1->flag && sa->v4->flag)
				*bigger= MIN2(*bigger, y1);
			else if(sa->v2->flag && sa->v3->flag)
				*smaller= MIN2(*smaller, y1);
		}
		else {
			int x1= sa->v4->vec.x - sa->v1->vec.x-AREAMINX;
			
			/* if left or right edge selected, test width */
			if(sa->v1->flag && sa->v2->flag)
				*bigger= MIN2(*bigger, x1);
			else if(sa->v3->flag && sa->v4->flag)
				*smaller= MIN2(*smaller, x1);
		}
	}
}

/* validate selection inside screen, set variables OK */
/* return 0: init failed */
static int move_areas_init (bContext *C, wmOperator *op)
{
	ScrEdge *actedge;
	sAreaMoveData *md;
	int x, y;

	/* required properties */
	if(!(OP_get_int(op, "x", &x) && OP_get_int(op, "y", &y)))
		return 0;

	/* default properties */
	OP_verify_int(op, "delta", 0, NULL);

	/* setup */
	actedge= screen_find_active_scredge(C->screen, x, y);
	if(actedge==NULL) return 0;

	md= MEM_callocN(sizeof(sAreaMoveData), "sAreaMoveData");
	op->customdata= md;

	md->dir= scredge_is_horizontal(actedge)?'h':'v';
	if(md->dir=='h') md->origval= actedge->v1->vec.y;
	else md->origval= actedge->v1->vec.x;
	
	select_connected_scredge(C->screen, actedge);
	/* now all vertices with 'flag==1' are the ones that can be moved. */

	move_areas_set_limits(C->screen, md->dir, &md->bigger, &md->smaller);
	
	return 1;
}

/* moves selected screen edge amount of delta, used by split & move */
static void move_areas_apply_do(bContext *C, int origval, int delta, int dir, int bigger, int smaller)
{
	ScrVert *v1;
	
	delta= CLAMPIS(delta, -smaller, bigger);
	
	for (v1= C->screen->vertbase.first; v1; v1= v1->next) {
		if (v1->flag) {
			/* that way a nice AREAGRID  */
			if((dir=='v') && v1->vec.x>0 && v1->vec.x<C->window->sizex-1) {
				v1->vec.x= origval + delta;
				if(delta != bigger && delta != -smaller) v1->vec.x-= (v1->vec.x % AREAGRID);
			}
			if((dir=='h') && v1->vec.y>0 && v1->vec.y<C->window->sizey-1) {
				v1->vec.y= origval + delta;

				v1->vec.y+= AREAGRID-1;
				v1->vec.y-= (v1->vec.y % AREAGRID);
				
				/* prevent too small top header */
				if(v1->vec.y > C->window->sizey-HEADERY)
					v1->vec.y= C->window->sizey-HEADERY;
			}
		}
	}

	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
}

static void move_areas_apply(bContext *C, wmOperator *op)
{
	sAreaMoveData *md= op->customdata;
	int delta;
	
	OP_get_int(op, "delta", &delta);
	move_areas_apply_do(C, md->origval, delta, md->dir, md->bigger, md->smaller);
}

static void move_areas_exit(bContext *C, wmOperator *op)
{
	if(op->customdata)
		MEM_freeN(op->customdata);
	op->customdata= NULL;
	
	/* this makes sure aligned edges will result in aligned grabbing */
	removedouble_scrverts(C->screen);
	removedouble_scredges(C->screen);
}

static int move_areas_exec(bContext *C, wmOperator *op)
{
	if(!move_areas_init(C, op))
		return OPERATOR_CANCELLED;
	
	move_areas_apply(C, op);
	move_areas_exit(C, op);
	
	return OPERATOR_FINISHED;
}

/* interaction callback */
static int move_areas_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	OP_verify_int(op, "x", event->x, NULL);
	OP_verify_int(op, "y", event->y, NULL);

	if(!move_areas_init(C, op)) 
		return OPERATOR_PASS_THROUGH;
	
	/* add temp handler */
	WM_event_add_modal_handler(&C->window->handlers, op);
	
	return OPERATOR_RUNNING_MODAL;
}

static int move_areas_cancel(bContext *C, wmOperator *op)
{
	WM_event_remove_modal_handler(&C->window->handlers, op);				

	OP_set_int(op, "delta", 0);
	move_areas_apply(C, op);
	move_areas_exit(C, op);

	return OPERATOR_CANCELLED;
}

/* modal callback for while moving edges */
static int move_areas_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	sAreaMoveData *md;
	int delta, x, y;

	md= op->customdata;

	OP_get_int(op, "x", &x);
	OP_get_int(op, "y", &y);

	/* execute the events */
	switch(event->type) {
		case MOUSEMOVE:
			delta= (md->dir == 'v')? event->x - x: event->y - y;
			OP_set_int(op, "delta", delta);

			move_areas_apply(C, op);
			break;
			
		case LEFTMOUSE:
			if(event->val==0) {
				move_areas_exit(C, op);
				WM_event_remove_modal_handler(&C->window->handlers, op);				
				return OPERATOR_FINISHED;
			}
			break;
			
		case ESCKEY:
			return move_areas_cancel(C, op);
	}
	
	return OPERATOR_RUNNING_MODAL;
}

void ED_SCR_OT_move_areas(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move area edges";
	ot->idname= "ED_SCR_OT_move_areas";

	ot->exec= move_areas_exec;
	ot->invoke= move_areas_invoke;
	ot->cancel= move_areas_cancel;
	ot->modal= move_areas_modal;

	ot->poll= ED_operator_screen_mainwinactive; /* when mouse is over area-edge */
}

/* ************** split area operator *********************************** */

/* 
operator state vars:  
	fac              spit point
	dir              direction 'v' or 'h'

operator customdata:
	area   			pointer to (active) area
	x, y			last used mouse pos
	(more, see below)

functions:

	init()   set default property values, find area based on context

	apply()	split area based on state vars

	exit()	cleanup, send notifier

	cancel() remove duplicated area

callbacks:

	exec()   execute without any user interaction, based on state vars
            call init(), apply(), exit()

	invoke() gets called on mouse click in action-widget
            call init(), add modal handler
			call apply() with initial motion

	modal()  accept modal events while doing it
            call move-areas code with delta motion
            call exit() or cancel() and remove handler

*/

#define SPLIT_STARTED	1
#define SPLIT_PROGRESS	2

typedef struct sAreaSplitData
{
	int x, y;	/* last used mouse position */
	
	int origval;			/* for move areas */
	int bigger, smaller;	/* constraints for moving new edge */
	int delta;				/* delta move edge */
	
	ScrEdge *nedge;			/* new edge */
	ScrArea *sarea;			/* start area */
	ScrArea *narea;			/* new area */
} sAreaSplitData;

/* generic init, no UI stuff here */
static int split_area_init(bContext *C, wmOperator *op)
{
	sAreaSplitData *sd;
	int dir;
	
	/* required context */
	if(C->area==NULL) return 0;
	
	/* required properties */
	OP_verify_float(op, "fac", 0.5f, NULL);
	OP_verify_int(op, "dir", 'h', &dir);
	
	/* minimal size */
	if(dir=='v' && C->area->winx < 2*AREAMINX) return 0;
	if(dir=='h' && C->area->winy < 2*AREAMINY) return 0;
	   
	/* custom data */
	sd= (sAreaSplitData*)MEM_callocN(sizeof (sAreaSplitData), "op_split_area");
	op->customdata= sd;
	
	sd->sarea= C->area;
	
	return 1;
}

/* do the split */
static void split_area_apply(bContext *C, wmOperator *op)
{
	sAreaSplitData *sd= (sAreaSplitData *)op->customdata;
	float fac;
	int dir;
	
	OP_get_float(op, "fac", &fac);
	OP_get_int(op, "dir", &dir);

	sd->narea= splitarea(C->window, C->screen, sd->sarea, dir, fac);
	
	if(sd->narea) {
		ScrVert *sv;
		
		sd->nedge= area_findsharededge(C->screen, sd->sarea, sd->narea);
	
		/* select newly created edge, prepare for moving edge */
		for(sv= C->screen->vertbase.first; sv; sv= sv->next)
			sv->flag = 0;
		
		sd->nedge->v1->flag= 1;
		sd->nedge->v2->flag= 1;

		if(dir=='h') sd->origval= sd->nedge->v1->vec.y;
		else sd->origval= sd->nedge->v1->vec.x;

	}		
	
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
	
}

static void split_area_exit(bContext *C, wmOperator *op)
{
	if (op->customdata) {
		MEM_freeN(op->customdata);
		op->customdata = NULL;
	}
	
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);

	/* this makes sure aligned edges will result in aligned grabbing */
	removedouble_scrverts(C->screen);
	removedouble_scredges(C->screen);
}


/* UI callback, adds new handler */
static int split_area_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	sAreaSplitData *sd;
	
	if(event->type==EVT_ACTIONZONE) {
		sActionzoneData *sad= event->customdata;
		int dir;
		
		/* verify *sad itself */
		if(sad==NULL || sad->sa1==NULL || sad->az==NULL)
			return OPERATOR_PASS_THROUGH;
		
		/* is this our *sad? if areas not equal it should be passed on */
		if(C->area!=sad->sa1 || sad->sa1!=sad->sa2)
			return OPERATOR_PASS_THROUGH;
		
		/* prepare operator state vars */
		if(sad->gesture_dir==AZONE_N || sad->gesture_dir==AZONE_S) {
			dir= 'h';
			OP_set_float(op, "fac", ((float)(event->x - sad->sa1->v1->vec.x)) / (float)sad->sa1->winx);
		}
		else {
			dir= 'v';
			OP_set_float(op, "fac", ((float)(event->y - sad->sa1->v1->vec.y)) / (float)sad->sa1->winy);
		}
		OP_set_int(op, "dir", dir);

		/* general init, also non-UI case, adds customdata, sets area and defaults */
		if(!split_area_init(C, op))
			return OPERATOR_PASS_THROUGH;
		
		sd= (sAreaSplitData *)op->customdata;
		
		sd->x= event->x;
		sd->y= event->y;
		
		/* do the split */
		split_area_apply(C, op);
		move_areas_set_limits(C->screen, dir, &sd->bigger, &sd->smaller);
		
		/* add temp handler for edge move or cancel */
		WM_event_add_modal_handler(&C->window->handlers, op);
		
		return OPERATOR_RUNNING_MODAL;
		
	}
	return OPERATOR_PASS_THROUGH;
}

/* function to be called outside UI context, or for redo */
static int split_area_exec(bContext *C, wmOperator *op)
{
	
	if(!split_area_init(C, op))
		return OPERATOR_CANCELLED;
	
	split_area_apply(C, op);
	split_area_exit(C, op);
	
	return OPERATOR_FINISHED;
}

static int split_area_cancel(bContext *C, wmOperator *op)
{
	sAreaSplitData *sd= (sAreaSplitData *)op->customdata;

	WM_event_remove_modal_handler(&C->window->handlers, op);

	if (screen_join_areas(C->screen,sd->sarea, sd->narea)) {
		if (C->area == sd->narea) {
			C->area = NULL;
		}
		sd->narea = NULL;
	}
	split_area_exit(C, op);

	return OPERATOR_CANCELLED;
}

static int split_area_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	sAreaSplitData *sd= (sAreaSplitData *)op->customdata;
	int dir;

	/* execute the events */
	switch(event->type) {
		case MOUSEMOVE:
			
			OP_get_int(op, "dir", &dir);
			
			sd->delta= (dir == 'v')? event->x - sd->origval: event->y - sd->origval;
			move_areas_apply_do(C, sd->origval, sd->delta, dir, sd->bigger, sd->smaller);
			
			WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
			break;
			
		case LEFTMOUSE:
			if(event->val==0) { /* mouse up */
				split_area_exit(C, op);
				WM_event_remove_modal_handler(&C->window->handlers, op);
				return OPERATOR_FINISHED;
			}
			break;
		case RIGHTMOUSE: /* cancel operation */
		case ESCKEY:
			return split_area_cancel(C, op);
	}
	
	return OPERATOR_RUNNING_MODAL;
}

void ED_SCR_OT_split_area(wmOperatorType *ot)
{
	ot->name = "Split area";
	ot->idname = "ED_SCR_OT_split_area";
	
	ot->exec= split_area_exec;
	ot->invoke= split_area_invoke;
	ot->modal= split_area_modal;
	
	ot->poll= ED_operator_screenactive; /* XXX should be area active */
}

/* ************** join area operator ********************************************** */

/* operator state vars used:  
			x1, y1     mouse coord in first area, which will disappear
			x2, y2     mouse coord in 2nd area, which will become joined

functions:

   init()   find edge based on state vars 
			test if the edge divides two areas, 
			store active and nonactive area,
            
   apply()  do the actual join

   exit()	cleanup, send notifier

callbacks:

   exec()	calls init, apply, exit 
   
   invoke() sets mouse coords in x,y
            call init()
            add modal handler

   modal()	accept modal events while doing it
			call apply() with active window and nonactive window
            call exit() and remove handler when LMB confirm

*/

typedef struct sAreaJoinData
{
	ScrArea *sa1;	/* first area to be considered */
	ScrArea *sa2;	/* second area to be considered */
	ScrArea *scr;	/* designed for removal */

} sAreaJoinData;


/* validate selection inside screen, set variables OK */
/* return 0: init failed */
/* XXX todo: find edge based on (x,y) and set other area? */
static int join_areas_init(bContext *C, wmOperator *op)
{
	ScrArea *sa1, *sa2;
	sAreaJoinData* jd= NULL;
	int x1, y1;
	int x2, y2;

	/* required properties, make negative to get return 0 if not set by caller */
	OP_verify_int(op, "x1", -100, &x1);
	OP_verify_int(op, "y1", -100, &y1);
	OP_verify_int(op, "x2", -100, &x2);
	OP_verify_int(op, "y2", -100, &y2);
	
	sa1 = screen_areahascursor(C->screen, x1, y1);
	sa2 = screen_areahascursor(C->screen, x2, y2);
	if(sa1==NULL || sa2==NULL || sa1==sa2)
		return 0;

	jd = (sAreaJoinData*)MEM_callocN(sizeof (sAreaJoinData), "op_join_areas");
		
	jd->sa1 = sa1;
	jd->sa1->flag |= AREA_FLAG_DRAWJOINFROM;
	jd->sa2 = sa2;
	jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
	
	op->customdata= jd;
	
	return 1;
}

/* apply the join of the areas (space types) */
static int join_areas_apply(bContext *C, wmOperator *op)
{
	sAreaJoinData *jd = (sAreaJoinData *)op->customdata;
	if (!jd) return 0;

	if(!screen_join_areas(C->screen,jd->sa1,jd->sa2)){
		return 0;
	}
	if (C->area == jd->sa2) {
		C->area = NULL;
	}

	return 1;
}

/* finish operation */
static void join_areas_exit(bContext *C, wmOperator *op)
{
	if (op->customdata) {
		MEM_freeN(op->customdata);
		op->customdata = NULL;
	}

	/* this makes sure aligned edges will result in aligned grabbing */
	removedouble_scredges(C->screen);
	removenotused_scredges(C->screen);
	removenotused_scrverts(C->screen);
}

static int join_areas_exec(bContext *C, wmOperator *op)
{
	if(!join_areas_init(C, op)) 
		return OPERATOR_CANCELLED;
	
	join_areas_apply(C, op);
	join_areas_exit(C, op);

	return OPERATOR_FINISHED;
}

/* interaction callback */
static int join_areas_invoke(bContext *C, wmOperator *op, wmEvent *event)
{

	if(event->type==EVT_ACTIONZONE) {
		sActionzoneData *sad= event->customdata;
		
		/* verify *sad itself */
		if(sad==NULL || sad->sa1==NULL || sad->sa2==NULL)
			return OPERATOR_PASS_THROUGH;
		
		/* is this our *sad? if areas equal it should be passed on */
		if(sad->sa1==sad->sa2)
			return OPERATOR_PASS_THROUGH;
		
		/* prepare operator state vars */
		OP_set_int(op, "x1", sad->x);
		OP_set_int(op, "y1", sad->y);
		OP_set_int(op, "x2", event->x);
		OP_set_int(op, "y2", event->y);

		if(!join_areas_init(C, op)) 
			return OPERATOR_PASS_THROUGH;
	
		/* add temp handler */
		WM_event_add_modal_handler(&C->window->handlers, op);
	
		return OPERATOR_RUNNING_MODAL;
	}
	
	return OPERATOR_PASS_THROUGH;
}

static int join_areas_cancel(bContext *C, wmOperator *op)
{
	sAreaJoinData *jd = (sAreaJoinData *)op->customdata;

	if (jd->sa1) {
		jd->sa1->flag &= ~AREA_FLAG_DRAWJOINFROM;
		jd->sa1->flag &= ~AREA_FLAG_DRAWJOINTO;
	}
	if (jd->sa2) {
		jd->sa2->flag &= ~AREA_FLAG_DRAWJOINFROM;
		jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
	}

	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
	WM_event_remove_modal_handler(&C->window->handlers, op);			
	
	join_areas_exit(C, op);

	return OPERATOR_CANCELLED;
}

/* modal callback while selecting area (space) that will be removed */
static int join_areas_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	sAreaJoinData *jd = (sAreaJoinData *)op->customdata;
	
	/* execute the events */
	switch(event->type) {
			
		case MOUSEMOVE: 
			{
				ScrArea *sa = screen_areahascursor(C->screen, event->x, event->y);
				int dir;
				
				if (sa) {					
					if (jd->sa1 != sa) {
						dir = area_getorientation(C->screen, jd->sa1, sa);
						if (dir >= 0) {
							if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
							jd->sa2 = sa;
							jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
						} 
						else {
							/* we are not bordering on the previously selected area 
							   we check if area has common border with the one marked for removal
							   in this case we can swap areas.
							*/
							dir = area_getorientation(C->screen, sa, jd->sa2);
							if (dir >= 0) {
								if (jd->sa1) jd->sa1->flag &= ~AREA_FLAG_DRAWJOINFROM;
								if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
								jd->sa1 = jd->sa2;
								jd->sa2 = sa;
								if (jd->sa1) jd->sa1->flag |= AREA_FLAG_DRAWJOINFROM;
								if (jd->sa2) jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
							} 
							else {
								if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
								jd->sa2 = NULL;
							}
						}
						WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
					} 
					else {
						/* we are back in the area previously selected for keeping 
						 * we swap the areas if possible to allow user to choose */
						if (jd->sa2 != NULL) {
							if (jd->sa1) jd->sa1->flag &= ~AREA_FLAG_DRAWJOINFROM;
							if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
							jd->sa1 = jd->sa2;
							jd->sa2 = sa;
							if (jd->sa1) jd->sa1->flag |= AREA_FLAG_DRAWJOINFROM;
							if (jd->sa2) jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
							dir = area_getorientation(C->screen, jd->sa1, jd->sa2);
							if (dir < 0) {
								printf("oops, didn't expect that!\n");
							}
						} 
						else {
							dir = area_getorientation(C->screen, jd->sa1, sa);
							if (dir >= 0) {
								if (jd->sa2) jd->sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
								jd->sa2 = sa;
								jd->sa2->flag |= AREA_FLAG_DRAWJOINTO;
							}
						}
						WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_WINDOW_REDRAW, 0, NULL);
					}
				}
			}
			break;
		case LEFTMOUSE:
			if(event->val==0) {
				join_areas_apply(C, op);
				WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0, NULL);
				join_areas_exit(C, op);
				WM_event_remove_modal_handler(&C->window->handlers, op);
				return OPERATOR_FINISHED;
			}
			break;
			
		case ESCKEY:
			return join_areas_cancel(C, op);
	}

	return OPERATOR_RUNNING_MODAL;
}

/* Operator for joining two areas (space types) */
void ED_SCR_OT_join_areas(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Join area";
	ot->idname= "ED_SCR_OT_join_areas";
	
	/* api callbacks */
	ot->exec= join_areas_exec;
	ot->invoke= join_areas_invoke;
	ot->cancel= join_areas_cancel;
	ot->modal= join_areas_modal;

	ot->poll= ED_operator_screenactive;
}

