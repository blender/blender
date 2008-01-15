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

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_area.h"
#include "ED_screen.h"

#include "wm_subwindow.h"

#include "screen_intern.h"	/* own module include */

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

static ScrArea *screen_test_edge_area(bScreen* scr, ScrArea *sa, ScrEdge *se)
{
	/* test if edge is in area, if not, 
	   then find an area that has it */
  
	ScrEdge *se1=0, *se2=0, *se3=0, *se4=0;
	
	if(sa) {
	se1= screen_findedge(scr, sa->v1, sa->v2);
		se2= screen_findedge(scr, sa->v2, sa->v3);
	se3= screen_findedge(scr, sa->v3, sa->v4);
		se4= screen_findedge(scr, sa->v4, sa->v1);
	}
	if(se1!=se && se2!=se && se3!=se && se4!=se) {
		
		sa= scr->areabase.first;
		while(sa) {
				/* a bit optimise? */
				if(se->v1==sa->v1 || se->v1==sa->v2 || se->v1==sa->v3 || se->v1==sa->v4) {
				se1= screen_findedge(scr, sa->v1, sa->v2);
					se2= screen_findedge(scr, sa->v2, sa->v3);
					se3= screen_findedge(scr, sa->v3, sa->v4);
					se4= screen_findedge(scr, sa->v4, sa->v1);
					if(se1==se || se2==se || se3==se || se4==se) return sa;
				}
				sa= sa->next;
			}
	}

	return sa;	/* is null when not find */
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
		if(se==0) printf("error: area %d edge 1 bestaat niet\n", a);
		else se->flag= 1;
		se= screen_findedge(sc, sa->v2, sa->v3);
		if(se==0) printf("error: area %d edge 2 bestaat niet\n", a);
		else se->flag= 1;
		se= screen_findedge(sc, sa->v3, sa->v4);
		if(se==0) printf("error: area %d edge 3 bestaat niet\n", a);
		else se->flag= 1;
		se= screen_findedge(sc, sa->v4, sa->v1);
		if(se==0) printf("error: area %d edge 4 bestaat niet\n", a);
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
	BKE_screen_area_free(sa);
	BLI_remlink(&sc->areabase, sa);
	MEM_freeN(sa);
}

bScreen *addscreen(wmWindow *win, char *name)
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
	
	screen_addarea(sc, sv1, sv2, sv3, sv4, HEADERDOWN, SPACE_INFO);
		
	return sc;
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
		sa->scriptlink.totscript= 0;
		
		area_copy_data(sa, saf, 0);
	}
	
	/* put at zero (needed?) */
	for(s1= from->vertbase.first; s1; s1= s1->next)
		s1->newv= NULL;

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
/* used with split operator */
static int area_getorientation(bScreen *screen, ScrArea *sa, ScrArea *sb)
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
		
		//XXX G.curscreen!
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

static ScrArea* splitarea(wmWindow *win, bScreen *sc, ScrArea *sa, char dir, float fac)
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
		if(sa->v1==sa->v2 || sa->v3==sa->v4 || sa->v2==sa->v3) {
			BKE_screen_area_free(sa);
			BLI_remlink(&sc->areabase, sa);
			MEM_freeN(sa);
		}
	}
}



#define SCR_BACK 0.55
#define SCR_ROUND 12

static void drawscredge_area(ScrArea *sa)
{
	short x1= sa->v1->vec.x;
	short y1= sa->v1->vec.y;
	short x2= sa->v3->vec.x;
	short y2= sa->v3->vec.y;
	
	cpack(0x0);
	
	/* right border area */
	sdrawline(x2, y1, x2, y2);
	
	/* left border area */
	if(x1>0) { // otherwise it draws the emboss of window over
		sdrawline(x1, y1, x1, y2);
	}	
	/* top border area */
	sdrawline(x1, y2, x2, y2);
	
	/* bottom border area */
	sdrawline(x1, y1, x2, y1);
}

void ED_screen_do_listen(bScreen *screen, wmNotifier *note)
{
	
	/* generic notes */
	switch(note->type) {
		case WM_NOTE_WINDOW_REDRAW:
			screen->do_draw= 1;
			break;
		case WM_NOTE_SCREEN_CHANGED:
			screen->do_draw= screen->do_refresh= 1;
			break;
	}
}


void ED_screen_draw(wmWindow *win)
{
	ScrArea *sa;
	
	wm_subwindow_set(win, win->screen->mainwin);
	
	for(sa= win->screen->areabase.first; sa; sa= sa->next)
		drawscredge_area(sa);

	printf("draw screen\n");
	win->screen->do_draw= 0;
}

/* make this screen usable */
/* for file read and first use, for scaling window, area moves */
void ED_screen_refresh(wmWindowManager *wm, wmWindow *win)
{
	ScrArea *sa;
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
	
	printf("set screen\n");
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
		WM_set_cursor(C, CURSOR_STD);
	}
	
	return 1;
}

/* ************** move area edge operator ********************************************** */

/* operator state vars used:  
           op->veci   mouse coord near edge
           op->delta  movement of edge

   callbacks:

   init()   find edge based on op->veci, test if the edge can be moved, select edges,
            clear delta, calculate min and max movement

   exec()	apply op->delta on selection
   
   invoke() handler gets called on a mouse click near edge
            call init()
            add handler

   modal()	accept modal events while doing it
			call exec() with delta motion
            call exit() and remove handler

   exit()	cleanup, send notifier

*/

/* validate selection inside screen, set variables OK */
/* return 0: init failed */
static int move_areas_init (bContext *C, wmOperator *op)
{
	ScrEdge *actedge= screen_find_active_scredge(C->screen, op->veci.x, op->veci.y);
	ScrArea *sa;
	int bigger, smaller, dir, origval;
	
	if(actedge==NULL) return 0;
	printf("move_areas_init\n");
	
	dir= scredge_is_horizontal(actedge)?'h':'v';
	if(dir=='h') origval= actedge->v1->vec.y;
	else origval= actedge->v1->vec.x;
	
	select_connected_scredge(C->screen, actedge);

	/* now all verices with 'flag==1' are the ones that can be moved. */
	/* we check all areas and test for free space with MINSIZE */
	bigger= smaller= 10000;
	for(sa= C->screen->areabase.first; sa; sa= sa->next) {
		if(dir=='h') {	/* if top or down edge selected, test height */
		   
		   if(sa->v1->flag && sa->v4->flag) {
			   int y1= sa->v2->vec.y - sa->v1->vec.y-AREAMINY;
			   bigger= MIN2(bigger, y1);
		   }
		   else if(sa->v2->flag && sa->v3->flag) {
			   int y1= sa->v2->vec.y - sa->v1->vec.y-AREAMINY;
			   smaller= MIN2(smaller, y1);
		   }
		}
		else {	/* if left or right edge selected, test width */
			if(sa->v1->flag && sa->v2->flag) {
				int x1= sa->v4->vec.x - sa->v1->vec.x-AREAMINX;
				bigger= MIN2(bigger, x1);
			}
			else if(sa->v3->flag && sa->v4->flag) {
				int x1= sa->v4->vec.x - sa->v1->vec.x-AREAMINX;
				smaller= MIN2(smaller, x1);
			}
		}
	}

	OP_set_int(op, "bigger", bigger);
	OP_set_int(op, "smaller", smaller);
	OP_set_int(op, "dir", dir);
	OP_set_int(op, "origval", origval);

	return 1;
}

/* moves selected screen edge amount of delta */
/* needs init call to work */
static int move_areas_exec(bContext *C, wmOperator *op)
{
	ScrVert *v1;
	int bigger, smaller, dir, origval;

	OP_get_int(op, "bigger", &bigger);
	OP_get_int(op, "smaller", &smaller);
	OP_get_int(op, "dir", &dir);
	OP_get_int(op, "origval", &origval);
	
	printf("move_areas_exec\n");
	
	op->delta= CLAMPIS(op->delta, -smaller, bigger);
	
	for (v1= C->screen->vertbase.first; v1; v1= v1->next) {
		if (v1->flag) {
			/* that way a nice AREAGRID  */
			if((dir=='v') && v1->vec.x>0 && v1->vec.x<C->window->sizex-1) {
				v1->vec.x= origval + op->delta;
				if(op->delta != bigger && op->delta != -smaller) v1->vec.x-= (v1->vec.x % AREAGRID);
			}
			if((dir=='h') && v1->vec.y>0 && v1->vec.y<C->window->sizey-1) {
				v1->vec.y= origval + op->delta;

				v1->vec.y+= AREAGRID-1;
				v1->vec.y-= (v1->vec.y % AREAGRID);
				
				/* prevent too small top header */
				if(v1->vec.y > C->window->sizey-HEADERY)
					v1->vec.y= C->window->sizey-HEADERY;
			}
		}
	}
	return 1;
}

static int move_areas_exit(bContext *C, wmOperator *op)
{
	printf("move_areas_exit\n");
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0);

	/* this makes sure aligned edges will result in aligned grabbing */
	removedouble_scrverts(C->screen);
	removedouble_scredges(C->screen);

	OP_free_property(op);	
	return 1;
}

/* interaction callback */
/* return 0 = stop evaluating for next handlers */
static int move_areas_invoke (bContext *C, wmOperator *op, wmEvent *event)
{
	
	/* operator arguments and storage */
	op->delta= 0;
	op->veci.x= event->x;
	op->veci.y= event->y;
	
	if(0==move_areas_init(C, op)) 
		return 1;
	
	printf("move_areas_invoke\n");
	
	/* add temp handler */
	WM_event_add_modal_handler(&C->window->handlers, op);
	
	return 0;
}

/* modal callback for while moving edges */
/* return 0 = stop evaluating for next handlers */
static int move_areas_modal (bContext *C, wmOperator *op, wmEvent *event)
{
	int dir;

	OP_get_int(op, "dir", &dir);

	printf("move_areas_modal\n");
	/* execute the events */
	switch(event->type) {
		case MOUSEMOVE:
			
			if(dir=='v')
				op->delta= event->x - op->veci.x;
			else
				op->delta= event->y - op->veci.y;
			
			move_areas_exec(C, op);
			WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0);
			break;
			
		case LEFTMOUSE:
			if(event->val==0) {
				WM_event_remove_modal_handler(&C->window->handlers, op);
				move_areas_exit(C, op);
			}
			break;
			
		case ESCKEY:
			op->delta= 0;
			move_areas_exec(C, op);
			
			WM_event_remove_modal_handler(&C->window->handlers, op);
			move_areas_exit(C, op);
			break;
	}
	
	return 1;
}

void ED_SCR_OT_move_areas(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Move area edges";
	ot->idname= "ED_SCR_OT_move_areas";

	ot->init= move_areas_init;
	ot->invoke= move_areas_invoke;
	ot->modal= move_areas_modal;
	ot->exec= move_areas_exec;
	ot->exit= move_areas_exit;

	ot->poll= ED_operator_screen_mainwinactive;
}

/****************** split area ********************/
/* we do split on init, then we work like move_areas
	if operation gets cancelled -> join
	if operation gets confirmed -> yay
*/

typedef struct sAreaSplitData
{
	int dir; /* direction of new edge */
	int origval; /* for move areas */
	int min,max; /* constraints for moving new edge */
	int pos; /* with sa as center, ne is located at: 0=W, 1=N, 2=E, 3=S */
	ScrEdge *nedge; /* new edge */
	ScrEdge *aedge;
	ScrArea *sarea; /* start area */
	ScrArea *narea; /* new area */
} sAreaSplitData;

/* the moving of the new egde */
static int split_area_exec(bContext *C, wmOperator *op)
{
	ScrVert *v1;
	sAreaSplitData *sd= (sAreaSplitData *)op->customdata;
	int newval= sd->origval + op->delta;
	printf("split_area_exec %d %c, %d %d, %d", op->delta, sd->dir, -sd->min, sd->max, sd->origval);
	
	op->delta= CLAMPIS(op->delta, -sd->min, sd->max);
	
	
	printf(".");
	/* that way a nice AREAGRID  */
	if((sd->dir=='v') && (newval > sd->min && newval < sd->max-1)) {
		sd->nedge->v1->vec.x= newval;
		sd->nedge->v2->vec.x= newval;
		//if(op->delta != sd->max && op->delta != -sd->min) v1->vec.x-= (v1->vec.x % AREAGRID);
	}
	if((sd->dir=='h') && (newval > sd->min+HEADERY && newval < sd->max-HEADERY)) {
		sd->nedge->v1->vec.y= sd->origval + op->delta;
		sd->nedge->v1->vec.y+= AREAGRID-1;
		sd->nedge->v1->vec.y-= (sd->nedge->v1->vec.y % AREAGRID);
		if(sd->nedge->v1->vec.y > C->window->sizey-HEADERY)
			sd->nedge->v1->vec.y= C->window->sizey-HEADERY;
		
		sd->nedge->v2->vec.y= sd->origval + op->delta;
		sd->nedge->v2->vec.y+= AREAGRID-1;
		sd->nedge->v2->vec.y-= (sd->nedge->v2->vec.y % AREAGRID);
		if(sd->nedge->v2->vec.y > C->window->sizey-HEADERY)
			sd->nedge->v2->vec.y= C->window->sizey-HEADERY;
	}
	printf("\n");
	return 1;
}

static int split_area_exit(bContext *C, wmOperator *op)
{
	printf("split_area_exit\n");
	if (op->customdata) {
		MEM_freeN(op->customdata);
		op->customdata = NULL;
	}
	
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0);

	/* this makes sure aligned edges will result in aligned grabbing */
	removedouble_scrverts(C->screen);
	removedouble_scredges(C->screen);
	
	return 1;
}

static void split_initintern(bContext *C, wmOperator *op, sAreaSplitData *sd)
{
	float fac= 0.0;
	if(sd->dir=='h') {
		sd->origval= op->veci.y;
		fac= 1.0 - ((float)(sd->sarea->v3->vec.y - op->veci.y)) / (float)sd->sarea->winy;
		sd->min= sd->aedge->v1->vec.y;
		sd->max= sd->aedge->v2->vec.y;
	}
	else {
		sd->origval= op->veci.x;
		fac= 1.0 - ((float)(sd->sarea->v4->vec.x - op->veci.x)) / (float)sd->sarea->winx;
		sd->min= sd->aedge->v1->vec.x;
		sd->max= sd->aedge->v2->vec.x;
	}
	
	sd->narea= splitarea(C->window, C->screen, sd->sarea, sd->dir, fac);
	sd->nedge= area_findsharededge(C->screen, sd->sarea, sd->narea);
	
	printf("split_area_init\n");
	/* get newly created edge and select it */
	select_connected_scredge(C->screen, sd->nedge);
	
}

static int split_area_init (bContext *C, wmOperator *op)
{
	float fac= 0.0;
	sAreaSplitData *sd= NULL;
	ScrEdge *actedge= screen_find_active_scredge(C->screen, op->veci.x, op->veci.y);
	
	if(actedge==NULL) return 0;
	
	sd= (sAreaSplitData*)MEM_callocN(sizeof (sAreaSplitData), "op_split_area");
	op->customdata= sd;
	/* for split dir is in which new edge comes */
	sd->dir= scredge_is_horizontal(actedge) ? 'v':'h';
	sd->sarea= screen_test_edge_area(C->screen, C->curarea, actedge);
	sd->aedge= actedge;

	split_initintern(C, op, sd);
	if(sd->narea == NULL) return 0;
	
	WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0);

	return 1;
}

static int split_area_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	/* operator arguments and storage */
	op->delta= 0;
	op->veci.x= event->x;
	op->veci.y= event->y;
	
	if(0==split_area_init(C, op)) 
		return 1;
	
	printf("split_area_invoke %d %d %d\n", op->delta, op->veci.x, op->veci.y);
	
	/* add temp handler */
	WM_event_add_modal_handler(&C->window->handlers, op);
	
	return 0;
}

static void split_joincurrent(bContext *C, sAreaSplitData *sd)
{
	int orientation= area_getorientation(C->window->screen, sd->sarea, sd->narea);
	if(orientation>-1) {
		if(orientation==0) {
			sd->sarea->v1= sd->narea->v1;
			sd->sarea->v2= sd->narea->v2;
			screen_addedge(C->screen, sd->sarea->v2, sd->sarea->v3);
			screen_addedge(C->screen, sd->sarea->v1, sd->sarea->v4);
		}
		else if(orientation==1) {
			sd->sarea->v2= sd->narea->v2;
			sd->sarea->v3= sd->narea->v3;
			screen_addedge(C->screen, sd->sarea->v1, sd->sarea->v2);
			screen_addedge(C->screen, sd->sarea->v3, sd->sarea->v4);
		}
		else if(orientation==2) {
			sd->sarea->v3= sd->narea->v3;
			sd->sarea->v4= sd->narea->v4;
			screen_addedge(C->screen, sd->sarea->v2,sd-> sarea->v3);
			screen_addedge(C->screen, sd->sarea->v1, sd->sarea->v4);
		}
		else if(orientation==3) {
			sd->sarea->v1= sd->narea->v1;
			sd->sarea->v4= sd->narea->v4;
			screen_addedge(C->screen, sd->sarea->v1, sd->sarea->v2);
			screen_addedge(C->screen, sd->sarea->v3, sd->sarea->v4);
		}

		if (C->curarea == sd->narea) {
			C->curarea = NULL;
		}
		screen_delarea(C->screen, sd->narea);
		sd->narea = NULL;
		removedouble_scrverts(C->screen);
		removedouble_scredges(C->screen);
	}
}

static int split_area_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	ScrArea *sa= NULL, *sold=NULL;
	int orientation= -1;
	sAreaSplitData *sd= (sAreaSplitData *)op->customdata;
	printf("split_area_modal\n");
	/* execute the events */
	switch(event->type) {
		case MOUSEMOVE:

			sa= screen_areahascursor(C->screen, event->x, event->y);

			if(sa && sd->sarea!=sa && sd->narea!=sa) {
				sold= sd->sarea;
				printf("In other area now\n");
				split_joincurrent(C, sd);
				//sd->aedge= area_findsharededge(C->screen, sold, sa);
				/* now find aedge with same orientation as sd->dir (inverted) */
				if(sd->dir=='v') sd->aedge= screen_findedge(C->screen, sa->v1, sa->v4);
				else sd->aedge= screen_findedge(C->screen, sa->v1, sa->v2);
				sd->sarea= sa;
				op->delta= 0;
				op->veci.x= event->x;
				op->veci.y= event->y;
				split_initintern(C, op, sd);
			}
			else {
				if(sd->dir=='v')
					op->delta= event->x - op->veci.x;
				else
					op->delta= event->y - op->veci.y;
				
				split_area_exec(C, op);
			}
			WM_event_add_notifier(C->wm, C->window, 0, WM_NOTE_SCREEN_CHANGED, 0);
			break;
			
		case RIGHTMOUSE:
			if(event->val==0) { /* mouse up */
				WM_event_remove_modal_handler(&C->window->handlers, op);
				split_area_exit(C, op);
			}
			break;
			
		case LEFTMOUSE:
		case ESCKEY:
			op->delta= 0;
			split_joincurrent(C, sd);
			WM_event_remove_modal_handler(&C->window->handlers, op);
			split_area_exit(C, op);
			break;
	}
	
	return 1;
}

void ED_SCR_OT_split_area(wmOperatorType *ot)
{
	ot->name = "Split area";
	ot->idname = "ED_SCR_OT_split_area";
	
	ot->init= split_area_init;
	ot->invoke= split_area_invoke;
	ot->modal= split_area_modal;
	ot->exec= NULL;//split_area_exec;
	ot->exit= split_area_exit;
	
	ot->poll= ED_operator_screenactive;
	//ot->poll= ED_operator_screen_mainwinactive;
}
