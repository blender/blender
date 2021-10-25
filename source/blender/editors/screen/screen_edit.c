/*
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/screen/screen_edit.c
 *  \ingroup edscr
 */


#include <string.h>
#include <math.h>


#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_screen.h"
#include "BKE_scene.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_screen_types.h"
#include "ED_clip.h"
#include "ED_node.h"
#include "ED_render.h"

#include "UI_interface.h"

/* XXX actually should be not here... solve later */
#include "wm_subwindow.h"

#include "screen_intern.h"  /* own module include */


/* ******************* screen vert, edge, area managing *********************** */

static ScrVert *screen_addvert(bScreen *sc, short x, short y)
{
	ScrVert *sv = MEM_callocN(sizeof(ScrVert), "addscrvert");
	sv->vec.x = x;
	sv->vec.y = y;
	
	BLI_addtail(&sc->vertbase, sv);
	return sv;
}

static void sortscrvert(ScrVert **v1, ScrVert **v2)
{
	ScrVert *tmp;
	
	if (*v1 > *v2) {
		tmp = *v1;
		*v1 = *v2;
		*v2 = tmp;
	}
}

static ScrEdge *screen_addedge(bScreen *sc, ScrVert *v1, ScrVert *v2)
{
	ScrEdge *se = MEM_callocN(sizeof(ScrEdge), "addscredge");
	
	sortscrvert(&v1, &v2);
	se->v1 = v1;
	se->v2 = v2;
	
	BLI_addtail(&sc->edgebase, se);
	return se;
}


ScrEdge *screen_findedge(bScreen *sc, ScrVert *v1, ScrVert *v2)
{
	ScrEdge *se;
	
	sortscrvert(&v1, &v2);
	for (se = sc->edgebase.first; se; se = se->next)
		if (se->v1 == v1 && se->v2 == v2)
			return se;
	
	return NULL;
}

void removedouble_scrverts(bScreen *sc)
{
	ScrVert *v1, *verg;
	ScrEdge *se;
	ScrArea *sa;
	
	verg = sc->vertbase.first;
	while (verg) {
		if (verg->newv == NULL) { /* !!! */
			v1 = verg->next;
			while (v1) {
				if (v1->newv == NULL) {   /* !?! */
					if (v1->vec.x == verg->vec.x && v1->vec.y == verg->vec.y) {
						/* printf("doublevert\n"); */
						v1->newv = verg;
					}
				}
				v1 = v1->next;
			}
		}
		verg = verg->next;
	}

	/* replace pointers in edges and faces */
	se = sc->edgebase.first;
	while (se) {
		if (se->v1->newv) se->v1 = se->v1->newv;
		if (se->v2->newv) se->v2 = se->v2->newv;
		/* edges changed: so.... */
		sortscrvert(&(se->v1), &(se->v2));
		se = se->next;
	}
	sa = sc->areabase.first;
	while (sa) {
		if (sa->v1->newv) sa->v1 = sa->v1->newv;
		if (sa->v2->newv) sa->v2 = sa->v2->newv;
		if (sa->v3->newv) sa->v3 = sa->v3->newv;
		if (sa->v4->newv) sa->v4 = sa->v4->newv;
		sa = sa->next;
	}

	/* remove */
	verg = sc->vertbase.first;
	while (verg) {
		v1 = verg->next;
		if (verg->newv) {
			BLI_remlink(&sc->vertbase, verg);
			MEM_freeN(verg);
		}
		verg = v1;
	}

}

void removenotused_scrverts(bScreen *sc)
{
	ScrVert *sv, *svn;
	ScrEdge *se;
	
	/* we assume edges are ok */
	
	se = sc->edgebase.first;
	while (se) {
		se->v1->flag = 1;
		se->v2->flag = 1;
		se = se->next;
	}
	
	sv = sc->vertbase.first;
	while (sv) {
		svn = sv->next;
		if (sv->flag == 0) {
			BLI_remlink(&sc->vertbase, sv);
			MEM_freeN(sv);
		}
		else {
			sv->flag = 0;
		}
		sv = svn;
	}
}

void removedouble_scredges(bScreen *sc)
{
	ScrEdge *verg, *se, *sn;
	
	/* compare */
	verg = sc->edgebase.first;
	while (verg) {
		se = verg->next;
		while (se) {
			sn = se->next;
			if (verg->v1 == se->v1 && verg->v2 == se->v2) {
				BLI_remlink(&sc->edgebase, se);
				MEM_freeN(se);
			}
			se = sn;
		}
		verg = verg->next;
	}
}

void removenotused_scredges(bScreen *sc)
{
	ScrEdge *se, *sen;
	ScrArea *sa;
	int a = 0;
	
	/* sets flags when edge is used in area */
	sa = sc->areabase.first;
	while (sa) {
		se = screen_findedge(sc, sa->v1, sa->v2);
		if (se == NULL) printf("error: area %d edge 1 doesn't exist\n", a);
		else se->flag = 1;
		se = screen_findedge(sc, sa->v2, sa->v3);
		if (se == NULL) printf("error: area %d edge 2 doesn't exist\n", a);
		else se->flag = 1;
		se = screen_findedge(sc, sa->v3, sa->v4);
		if (se == NULL) printf("error: area %d edge 3 doesn't exist\n", a);
		else se->flag = 1;
		se = screen_findedge(sc, sa->v4, sa->v1);
		if (se == NULL) printf("error: area %d edge 4 doesn't exist\n", a);
		else se->flag = 1;
		sa = sa->next;
		a++;
	}
	se = sc->edgebase.first;
	while (se) {
		sen = se->next;
		if (se->flag == 0) {
			BLI_remlink(&sc->edgebase, se);
			MEM_freeN(se);
		}
		else {
			se->flag = 0;
		}
		se = sen;
	}
}

bool scredge_is_horizontal(ScrEdge *se)
{
	return (se->v1->vec.y == se->v2->vec.y);
}

/* need win size to make sure not to include edges along screen edge */
ScrEdge *screen_find_active_scredge(bScreen *sc,
                                    const int winsize_x, const int winsize_y,
                                    const int mx, const int my)
{
	ScrEdge *se;
	int safety = U.widget_unit / 10;
	
	if (safety < 2) safety = 2;
	
	for (se = sc->edgebase.first; se; se = se->next) {
		if (scredge_is_horizontal(se)) {
			if (se->v1->vec.y > 0 && se->v1->vec.y < winsize_y - 1) {
				short min, max;
				min = MIN2(se->v1->vec.x, se->v2->vec.x);
				max = MAX2(se->v1->vec.x, se->v2->vec.x);
				
				if (abs(my - se->v1->vec.y) <= safety && mx >= min && mx <= max)
					return se;
			}
		}
		else {
			if (se->v1->vec.x > 0 && se->v1->vec.x < winsize_x - 1) {
				short min, max;
				min = MIN2(se->v1->vec.y, se->v2->vec.y);
				max = MAX2(se->v1->vec.y, se->v2->vec.y);
				
				if (abs(mx - se->v1->vec.x) <= safety && my >= min && my <= max)
					return se;
			}
		}
	}
	
	return NULL;
}



/* adds no space data */
static ScrArea *screen_addarea(bScreen *sc, ScrVert *v1, ScrVert *v2, ScrVert *v3, ScrVert *v4, short headertype, short spacetype)
{
	ScrArea *sa = MEM_callocN(sizeof(ScrArea), "addscrarea");
	sa->v1 = v1;
	sa->v2 = v2;
	sa->v3 = v3;
	sa->v4 = v4;
	sa->headertype = headertype;
	sa->spacetype = sa->butspacetype = spacetype;
	
	BLI_addtail(&sc->areabase, sa);
	
	return sa;
}

static void screen_delarea(bContext *C, bScreen *sc, ScrArea *sa)
{
	
	ED_area_exit(C, sa);
	
	BKE_screen_area_free(sa);
	
	BLI_remlink(&sc->areabase, sa);
	MEM_freeN(sa);
}

/* return 0: no split possible */
/* else return (integer) screencoordinate split point */
static short testsplitpoint(ScrArea *sa, char dir, float fac)
{
	short x, y;
	const short area_min_x = AREAMINX;
	const short area_min_y = ED_area_headersize();
	
	// area big enough?
	if (dir == 'v' && (sa->v4->vec.x - sa->v1->vec.x <= 2 * area_min_x)) return 0;
	if (dir == 'h' && (sa->v2->vec.y - sa->v1->vec.y <= 2 * area_min_y)) return 0;
	
	// to be sure
	CLAMP(fac, 0.0f, 1.0f);
	
	if (dir == 'h') {
		y = sa->v1->vec.y + fac * (sa->v2->vec.y - sa->v1->vec.y);
		
		if (y - sa->v1->vec.y < area_min_y)
			y = sa->v1->vec.y + area_min_y;
		else if (sa->v2->vec.y - y < area_min_y)
			y = sa->v2->vec.y - area_min_y;
		else y -= (y % AREAGRID);
		
		return y;
	}
	else {
		x = sa->v1->vec.x + fac * (sa->v4->vec.x - sa->v1->vec.x);
		
		if (x - sa->v1->vec.x < area_min_x)
			x = sa->v1->vec.x + area_min_x;
		else if (sa->v4->vec.x - x < area_min_x)
			x = sa->v4->vec.x - area_min_x;
		else x -= (x % AREAGRID);
		
		return x;
	}
}

ScrArea *area_split(bScreen *sc, ScrArea *sa, char dir, float fac, int merge)
{
	ScrArea *newa = NULL;
	ScrVert *sv1, *sv2;
	short split;
	
	if (sa == NULL) return NULL;
	
	split = testsplitpoint(sa, dir, fac);
	if (split == 0) return NULL;
	
	/* note regarding (fac > 0.5f) checks below.
	 * normally it shouldn't matter which is used since the copy should match the original
	 * however with viewport rendering and python console this isn't the case. - campbell */

	if (dir == 'h') {
		/* new vertices */
		sv1 = screen_addvert(sc, sa->v1->vec.x, split);
		sv2 = screen_addvert(sc, sa->v4->vec.x, split);
		
		/* new edges */
		screen_addedge(sc, sa->v1, sv1);
		screen_addedge(sc, sv1, sa->v2);
		screen_addedge(sc, sa->v3, sv2);
		screen_addedge(sc, sv2, sa->v4);
		screen_addedge(sc, sv1, sv2);
		
		if (fac > 0.5f) {
			/* new areas: top */
			newa = screen_addarea(sc, sv1, sa->v2, sa->v3, sv2, sa->headertype, sa->spacetype);

			/* area below */
			sa->v2 = sv1;
			sa->v3 = sv2;
		}
		else {
			/* new areas: bottom */
			newa = screen_addarea(sc, sa->v1, sv1, sv2, sa->v4, sa->headertype, sa->spacetype);

			/* area above */
			sa->v1 = sv1;
			sa->v4 = sv2;
		}

		ED_area_data_copy(newa, sa, true);
		
	}
	else {
		/* new vertices */
		sv1 = screen_addvert(sc, split, sa->v1->vec.y);
		sv2 = screen_addvert(sc, split, sa->v2->vec.y);
		
		/* new edges */
		screen_addedge(sc, sa->v1, sv1);
		screen_addedge(sc, sv1, sa->v4);
		screen_addedge(sc, sa->v2, sv2);
		screen_addedge(sc, sv2, sa->v3);
		screen_addedge(sc, sv1, sv2);
		
		if (fac > 0.5f) {
			/* new areas: right */
			newa = screen_addarea(sc, sv1, sv2, sa->v3, sa->v4, sa->headertype, sa->spacetype);

			/* area left */
			sa->v3 = sv2;
			sa->v4 = sv1;
		}
		else {
			/* new areas: left */
			newa = screen_addarea(sc, sa->v1, sa->v2, sv2, sv1, sa->headertype, sa->spacetype);

			/* area right */
			sa->v1 = sv1;
			sa->v2 = sv2;
		}

		ED_area_data_copy(newa, sa, true);
	}
	
	/* remove double vertices en edges */
	if (merge)
		removedouble_scrverts(sc);
	removedouble_scredges(sc);
	removenotused_scredges(sc);
	
	return newa;
}

/* empty screen, with 1 dummy area without spacedata */
/* uses window size */
bScreen *ED_screen_add(wmWindow *win, Scene *scene, const char *name)
{
	const int winsize_x = WM_window_pixels_x(win);
	const int winsize_y = WM_window_pixels_y(win);

	bScreen *sc;
	ScrVert *sv1, *sv2, *sv3, *sv4;
	
	sc = BKE_libblock_alloc(G.main, ID_SCR, name);
	sc->scene = scene;
	sc->do_refresh = true;
	sc->redraws_flag = TIME_ALL_3D_WIN | TIME_ALL_ANIM_WIN;
	sc->winid = win->winid;

	sv1 = screen_addvert(sc, 0, 0);
	sv2 = screen_addvert(sc, 0, winsize_y - 1);
	sv3 = screen_addvert(sc, winsize_x - 1, winsize_y - 1);
	sv4 = screen_addvert(sc, winsize_x - 1, 0);
	
	screen_addedge(sc, sv1, sv2);
	screen_addedge(sc, sv2, sv3);
	screen_addedge(sc, sv3, sv4);
	screen_addedge(sc, sv4, sv1);
	
	/* dummy type, no spacedata */
	screen_addarea(sc, sv1, sv2, sv3, sv4, HEADERDOWN, SPACE_EMPTY);
		
	return sc;
}

static void screen_copy(bScreen *to, bScreen *from)
{
	ScrVert *s1, *s2;
	ScrEdge *se;
	ScrArea *sa, *saf;
	
	/* free contents of 'to', is from blenkernel screen.c */
	BKE_screen_free(to);
	
	BLI_duplicatelist(&to->vertbase, &from->vertbase);
	BLI_duplicatelist(&to->edgebase, &from->edgebase);
	BLI_duplicatelist(&to->areabase, &from->areabase);
	BLI_listbase_clear(&to->regionbase);
	
	s2 = to->vertbase.first;
	for (s1 = from->vertbase.first; s1; s1 = s1->next, s2 = s2->next) {
		s1->newv = s2;
	}
	
	for (se = to->edgebase.first; se; se = se->next) {
		se->v1 = se->v1->newv;
		se->v2 = se->v2->newv;
		sortscrvert(&(se->v1), &(se->v2));
	}
	
	saf = from->areabase.first;
	for (sa = to->areabase.first; sa; sa = sa->next, saf = saf->next) {
		sa->v1 = sa->v1->newv;
		sa->v2 = sa->v2->newv;
		sa->v3 = sa->v3->newv;
		sa->v4 = sa->v4->newv;

		BLI_listbase_clear(&sa->spacedata);
		BLI_listbase_clear(&sa->regionbase);
		BLI_listbase_clear(&sa->actionzones);
		BLI_listbase_clear(&sa->handlers);
		
		ED_area_data_copy(sa, saf, true);
	}
	
	/* put at zero (needed?) */
	for (s1 = from->vertbase.first; s1; s1 = s1->next)
		s1->newv = NULL;

}


/* with sa as center, sb is located at: 0=W, 1=N, 2=E, 3=S */
/* -1 = not valid check */
/* used with join operator */
int area_getorientation(ScrArea *sa, ScrArea *sb)
{
	ScrVert *sav1, *sav2, *sav3, *sav4;
	ScrVert *sbv1, *sbv2, *sbv3, *sbv4;

	if (sa == NULL || sb == NULL) return -1;

	sav1 = sa->v1;
	sav2 = sa->v2;
	sav3 = sa->v3;
	sav4 = sa->v4;
	sbv1 = sb->v1;
	sbv2 = sb->v2;
	sbv3 = sb->v3;
	sbv4 = sb->v4;
	
	if (sav1 == sbv4 && sav2 == sbv3) { /* sa to right of sb = W */
		return 0;
	}
	else if (sav2 == sbv1 && sav3 == sbv4) { /* sa to bottom of sb = N */
		return 1;
	}
	else if (sav3 == sbv2 && sav4 == sbv1) { /* sa to left of sb = E */
		return 2;
	}
	else if (sav1 == sbv2 && sav4 == sbv3) { /* sa on top of sb = S*/
		return 3;
	}
	
	return -1;
}

/* Helper function to join 2 areas, it has a return value, 0=failed 1=success
 *  used by the split, join operators
 */
int screen_area_join(bContext *C, bScreen *scr, ScrArea *sa1, ScrArea *sa2)
{
	int dir;
	
	dir = area_getorientation(sa1, sa2);
	/*printf("dir is : %i\n", dir);*/
	
	if (dir == -1) {
		if (sa1) sa1->flag &= ~AREA_FLAG_DRAWJOINFROM;
		if (sa2) sa2->flag &= ~AREA_FLAG_DRAWJOINTO;
		return 0;
	}
	
	if (dir == 0) {
		sa1->v1 = sa2->v1;
		sa1->v2 = sa2->v2;
		screen_addedge(scr, sa1->v2, sa1->v3);
		screen_addedge(scr, sa1->v1, sa1->v4);
	}
	else if (dir == 1) {
		sa1->v2 = sa2->v2;
		sa1->v3 = sa2->v3;
		screen_addedge(scr, sa1->v1, sa1->v2);
		screen_addedge(scr, sa1->v3, sa1->v4);
	}
	else if (dir == 2) {
		sa1->v3 = sa2->v3;
		sa1->v4 = sa2->v4;
		screen_addedge(scr, sa1->v2, sa1->v3);
		screen_addedge(scr, sa1->v1, sa1->v4);
	}
	else if (dir == 3) {
		sa1->v1 = sa2->v1;
		sa1->v4 = sa2->v4;
		screen_addedge(scr, sa1->v1, sa1->v2);
		screen_addedge(scr, sa1->v3, sa1->v4);
	}
	
	screen_delarea(C, scr, sa2);
	removedouble_scrverts(scr);
	sa1->flag &= ~AREA_FLAG_DRAWJOINFROM;
	
	return 1;
}

void select_connected_scredge(bScreen *sc, ScrEdge *edge)
{
	ScrEdge *se;
	ScrVert *sv;
	int oneselected;
	char dir;
	
	/* select connected, only in the right direction */
	/* 'dir' is the direction of EDGE */
	
	if (edge->v1->vec.x == edge->v2->vec.x) dir = 'v';
	else dir = 'h';
	
	sv = sc->vertbase.first;
	while (sv) {
		sv->flag = 0;
		sv = sv->next;
	}
	
	edge->v1->flag = 1;
	edge->v2->flag = 1;
	
	oneselected = 1;
	while (oneselected) {
		se = sc->edgebase.first;
		oneselected = 0;
		while (se) {
			if (se->v1->flag + se->v2->flag == 1) {
				if (dir == 'h') {
					if (se->v1->vec.y == se->v2->vec.y) {
						se->v1->flag = se->v2->flag = 1;
						oneselected = 1;
					}
				}
				if (dir == 'v') {
					if (se->v1->vec.x == se->v2->vec.x) {
						se->v1->flag = se->v2->flag = 1;
						oneselected = 1;
					}
				}
			}
			se = se->next;
		}
	}
}

/* test if screen vertices should be scaled */
static void screen_test_scale(bScreen *sc, int winsize_x, int winsize_y)
{
	/* clamp Y size of header sized areas when expanding windows
	 * avoids annoying empty space around file menu */
#define USE_HEADER_SIZE_CLAMP

	const int headery_init = ED_area_headersize();
	ScrVert *sv = NULL;
	ScrArea *sa;
	int winsize_x_prev, winsize_y_prev;
	float facx, facy, tempf, min[2], max[2];
	
	/* calculate size */
	min[0] = min[1] = 20000.0f;
	max[0] = max[1] = 0.0f;
	
	for (sv = sc->vertbase.first; sv; sv = sv->next) {
		const float fv[2] = {(float)sv->vec.x, (float)sv->vec.y};
		minmax_v2v2_v2(min, max, fv);
	}
	
	/* always make 0.0 left under */
	for (sv = sc->vertbase.first; sv; sv = sv->next) {
		sv->vec.x -= min[0];
		sv->vec.y -= min[1];
	}
	
	winsize_x_prev = (max[0] - min[0]) + 1;
	winsize_y_prev = (max[1] - min[1]) + 1;


#ifdef USE_HEADER_SIZE_CLAMP
#define TEMP_BOTTOM 1
#define TEMP_TOP 2

	/* if the window's Y axis grows, clamp header sized areas */
	if (winsize_y_prev < winsize_y) {  /* growing? */
		const int headery_margin_max = headery_init + 4;
		for (sa = sc->areabase.first; sa; sa = sa->next) {
			ARegion *ar = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);
			sa->temp = 0;

			if (ar && !(ar->flag & RGN_FLAG_HIDDEN)) {
				if (sa->v2->vec.y == winsize_y_prev - 1) {
					if ((sa->v2->vec.y - sa->v1->vec.y) < headery_margin_max) {
						sa->temp = TEMP_TOP;
					}
				}
				else if (sa->v1->vec.y == 0) {
					if ((sa->v2->vec.y - sa->v1->vec.y) < headery_margin_max) {
						sa->temp = TEMP_BOTTOM;
					}
				}
			}
		}
	}
#endif


	if (winsize_x_prev != winsize_x || winsize_y_prev != winsize_y) {
		facx = ((float)winsize_x - 1) / ((float)winsize_x_prev - 1);
		facy = ((float)winsize_y - 1) / ((float)winsize_y_prev - 1);
		
		/* make sure it fits! */
		for (sv = sc->vertbase.first; sv; sv = sv->next) {
			/* FIXME, this re-sizing logic is no good when re-sizing the window + redrawing [#24428]
			 * need some way to store these as floats internally and re-apply from there. */
			tempf = ((float)sv->vec.x) * facx;
			sv->vec.x = (short)(tempf + 0.5f);
			//sv->vec.x += AREAGRID - 1;
			//sv->vec.x -=  (sv->vec.x % AREAGRID);

			CLAMP(sv->vec.x, 0, winsize_x - 1);
			
			tempf = ((float)sv->vec.y) * facy;
			sv->vec.y = (short)(tempf + 0.5f);
			//sv->vec.y += AREAGRID - 1;
			//sv->vec.y -=  (sv->vec.y % AREAGRID);

			CLAMP(sv->vec.y, 0, winsize_y - 1);
		}
	}


#ifdef USE_HEADER_SIZE_CLAMP
	if (winsize_y_prev < winsize_y) {  /* growing? */
		for (sa = sc->areabase.first; sa; sa = sa->next) {
			ScrEdge *se = NULL;

			if (sa->temp == 0)
				continue;

			if (sa->v1 == sa->v2)
				continue;

			/* adjust headery if verts are along the edge of window */
			if (sa->temp == TEMP_TOP) {
				/* lower edge */
				const int yval = sa->v2->vec.y - headery_init;
				se = screen_findedge(sc, sa->v4, sa->v1);
				if (se != NULL) {
					select_connected_scredge(sc, se);
				}
				for (sv = sc->vertbase.first; sv; sv = sv->next) {
					if (sv != sa->v2 && sv != sa->v3) {
						if (sv->flag) {
							sv->vec.y = yval;
						}
					}
				}
			}
			else {
				/* upper edge */
				const int yval = sa->v1->vec.y + headery_init;
				se = screen_findedge(sc, sa->v2, sa->v3);
				if (se != NULL) {
					select_connected_scredge(sc, se);
				}
				for (sv = sc->vertbase.first; sv; sv = sv->next) {
					if (sv != sa->v1 && sv != sa->v4) {
						if (sv->flag) {
							sv->vec.y = yval;
						}
					}
				}
			}
		}
	}

#undef USE_HEADER_SIZE_CLAMP
#undef TEMP_BOTTOM
#undef TEMP_TOP
#endif


	/* test for collapsed areas. This could happen in some blender version... */
	/* ton: removed option now, it needs Context... */
	
	/* make each window at least ED_area_headersize() high */
	for (sa = sc->areabase.first; sa; sa = sa->next) {
		int headery = headery_init;
		
		/* adjust headery if verts are along the edge of window */
		if (sa->v1->vec.y > 0)
			headery += U.pixelsize;
		if (sa->v2->vec.y < winsize_y - 1)
			headery += U.pixelsize;
		
		if (sa->v2->vec.y - sa->v1->vec.y + 1 < headery) {
			/* lower edge */
			ScrEdge *se = screen_findedge(sc, sa->v4, sa->v1);
			if (se && sa->v1 != sa->v2) {
				int yval;
				
				select_connected_scredge(sc, se);
				
				/* all selected vertices get the right offset */
				yval = sa->v2->vec.y - headery + 1;
				for (sv = sc->vertbase.first; sv; sv = sv->next) {
					/* if is a collapsed area */
					if (sv != sa->v2 && sv != sa->v3) {
						if (sv->flag) {
							sv->vec.y = yval;
						}
					}
				}
			}
		}
	}
	
}

/* *********************** DRAWING **************************************** */

/* draw vertical shape visualizing future joining (left as well
 * right direction of future joining) */
static void draw_horizontal_join_shape(ScrArea *sa, char dir)
{
	vec2f points[10];
	short i;
	float w, h;
	float width = sa->v3->vec.x - sa->v1->vec.x;
	float height = sa->v3->vec.y - sa->v1->vec.y;

	if (height < width) {
		h = height / 8;
		w = height / 4;
	}
	else {
		h = width / 8;
		w = width / 4;
	}

	points[0].x = sa->v1->vec.x;
	points[0].y = sa->v1->vec.y + height / 2;

	points[1].x = sa->v1->vec.x;
	points[1].y = sa->v1->vec.y;

	points[2].x = sa->v4->vec.x - w;
	points[2].y = sa->v4->vec.y;

	points[3].x = sa->v4->vec.x - w;
	points[3].y = sa->v4->vec.y + height / 2 - 2 * h;

	points[4].x = sa->v4->vec.x - 2 * w;
	points[4].y = sa->v4->vec.y + height / 2;

	points[5].x = sa->v4->vec.x - w;
	points[5].y = sa->v4->vec.y + height / 2 + 2 * h;

	points[6].x = sa->v3->vec.x - w;
	points[6].y = sa->v3->vec.y;

	points[7].x = sa->v2->vec.x;
	points[7].y = sa->v2->vec.y;

	points[8].x = sa->v4->vec.x;
	points[8].y = sa->v4->vec.y + height / 2 - h;

	points[9].x = sa->v4->vec.x;
	points[9].y = sa->v4->vec.y + height / 2 + h;

	if (dir == 'l') {
		/* when direction is left, then we flip direction of arrow */
		float cx = sa->v1->vec.x + width;
		for (i = 0; i < 10; i++) {
			points[i].x -= cx;
			points[i].x = -points[i].x;
			points[i].x += sa->v1->vec.x;
		}
	}

	glBegin(GL_POLYGON);
	for (i = 0; i < 5; i++)
		glVertex2f(points[i].x, points[i].y);
	glEnd();
	glBegin(GL_POLYGON);
	for (i = 4; i < 8; i++)
		glVertex2f(points[i].x, points[i].y);
	glVertex2f(points[0].x, points[0].y);
	glEnd();

	glRectf(points[2].x, points[2].y, points[8].x, points[8].y);
	glRectf(points[6].x, points[6].y, points[9].x, points[9].y);
}

/* draw vertical shape visualizing future joining (up/down direction) */
static void draw_vertical_join_shape(ScrArea *sa, char dir)
{
	vec2f points[10];
	short i;
	float w, h;
	float width = sa->v3->vec.x - sa->v1->vec.x;
	float height = sa->v3->vec.y - sa->v1->vec.y;

	if (height < width) {
		h = height / 4;
		w = height / 8;
	}
	else {
		h = width / 4;
		w = width / 8;
	}

	points[0].x = sa->v1->vec.x + width / 2;
	points[0].y = sa->v3->vec.y;

	points[1].x = sa->v2->vec.x;
	points[1].y = sa->v2->vec.y;

	points[2].x = sa->v1->vec.x;
	points[2].y = sa->v1->vec.y + h;

	points[3].x = sa->v1->vec.x + width / 2 - 2 * w;
	points[3].y = sa->v1->vec.y + h;

	points[4].x = sa->v1->vec.x + width / 2;
	points[4].y = sa->v1->vec.y + 2 * h;

	points[5].x = sa->v1->vec.x + width / 2 + 2 * w;
	points[5].y = sa->v1->vec.y + h;

	points[6].x = sa->v4->vec.x;
	points[6].y = sa->v4->vec.y + h;
	
	points[7].x = sa->v3->vec.x;
	points[7].y = sa->v3->vec.y;

	points[8].x = sa->v1->vec.x + width / 2 - w;
	points[8].y = sa->v1->vec.y;

	points[9].x = sa->v1->vec.x + width / 2 + w;
	points[9].y = sa->v1->vec.y;

	if (dir == 'u') {
		/* when direction is up, then we flip direction of arrow */
		float cy = sa->v1->vec.y + height;
		for (i = 0; i < 10; i++) {
			points[i].y -= cy;
			points[i].y = -points[i].y;
			points[i].y += sa->v1->vec.y;
		}
	}

	glBegin(GL_POLYGON);
	for (i = 0; i < 5; i++)
		glVertex2f(points[i].x, points[i].y);
	glEnd();
	glBegin(GL_POLYGON);
	for (i = 4; i < 8; i++)
		glVertex2f(points[i].x, points[i].y);
	glVertex2f(points[0].x, points[0].y);
	glEnd();

	glRectf(points[2].x, points[2].y, points[8].x, points[8].y);
	glRectf(points[6].x, points[6].y, points[9].x, points[9].y);
}

/* draw join shape due to direction of joining */
static void draw_join_shape(ScrArea *sa, char dir)
{
	if (dir == 'u' || dir == 'd')
		draw_vertical_join_shape(sa, dir);
	else
		draw_horizontal_join_shape(sa, dir);
}

/* draw screen area darker with arrow (visualization of future joining) */
static void scrarea_draw_shape_dark(ScrArea *sa, char dir)
{
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glColor4ub(0, 0, 0, 50);
	draw_join_shape(sa, dir);
}

/* draw screen area ligher with arrow shape ("eraser" of previous dark shape) */
static void scrarea_draw_shape_light(ScrArea *sa, char UNUSED(dir))
{
	glBlendFunc(GL_DST_COLOR, GL_SRC_ALPHA);
	/* value 181 was hardly computed: 181~105 */
	glColor4ub(255, 255, 255, 50);
	/* draw_join_shape(sa, dir); */
	glRecti(sa->v1->vec.x, sa->v1->vec.y, sa->v3->vec.x, sa->v3->vec.y);
}

static void drawscredge_area_draw(int sizex, int sizey, short x1, short y1, short x2, short y2)
{
	/* right border area */
	if (x2 < sizex - 1) {
		glVertex2s(x2, y1);
		glVertex2s(x2, y2);
	}

	/* left border area */
	if (x1 > 0) { /* otherwise it draws the emboss of window over */
		glVertex2s(x1, y1);
		glVertex2s(x1, y2);
	}

	/* top border area */
	if (y2 < sizey - 1) {
		glVertex2s(x1, y2);
		glVertex2s(x2, y2);
	}

	/* bottom border area */
	if (y1 > 0) {
		glVertex2s(x1, y1);
		glVertex2s(x2, y1);
	}
}

/** screen edges drawing **/
static void drawscredge_area(ScrArea *sa, int sizex, int sizey)
{
	short x1 = sa->v1->vec.x;
	short y1 = sa->v1->vec.y;
	short x2 = sa->v3->vec.x;
	short y2 = sa->v3->vec.y;
	
	drawscredge_area_draw(sizex, sizey, x1, y1, x2, y2);
}

/* ****************** EXPORTED API TO OTHER MODULES *************************** */

bScreen *ED_screen_duplicate(wmWindow *win, bScreen *sc)
{
	bScreen *newsc;
	
	if (sc->state != SCREENNORMAL) return NULL;  /* XXX handle this case! */
	
	/* make new empty screen: */
	newsc = ED_screen_add(win, sc->scene, sc->id.name + 2);
	/* copy all data */
	screen_copy(newsc, sc);

	return newsc;
}

/* screen sets cursor based on swinid */
static void region_cursor_set(wmWindow *win, int swinid, int swin_changed)
{
	for (ScrArea *sa = win->screen->areabase.first; sa; sa = sa->next) {
		for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->swinid == swinid) {
				if (swin_changed || (ar->type && ar->type->event_cursor)) {
					ED_region_cursor_set(win, sa, ar);
				}
				return;
			}
		}
	}
}

void ED_screen_do_listen(bContext *C, wmNotifier *note)
{
	wmWindow *win = CTX_wm_window(C);
	
	/* generic notes */
	switch (note->category) {
		case NC_WM:
			if (note->data == ND_FILEREAD)
				win->screen->do_draw = true;
			break;
		case NC_WINDOW:
			win->screen->do_draw = true;
			break;
		case NC_SCREEN:
			if (note->action == NA_EDITED)
				win->screen->do_draw = win->screen->do_refresh = true;
			break;
		case NC_SCENE:
			if (note->data == ND_MODE)
				region_cursor_set(win, note->swinid, true);
			break;
	}
}

/* only for edge lines between areas, and the blended join arrows */
void ED_screen_draw(wmWindow *win)
{
	const int winsize_x = WM_window_pixels_x(win);
	const int winsize_y = WM_window_pixels_y(win);

	ScrArea *sa;
	ScrArea *sa1 = NULL;
	ScrArea *sa2 = NULL;
	ScrArea *sa3 = NULL;

	wmSubWindowSet(win, win->screen->mainwin);
	
	/* Note: first loop only draws if U.pixelsize > 1, skip otherwise */
	if (U.pixelsize > 1.0f) {
		/* FIXME: doesn't our glLineWidth already scale by U.pixelsize? */
		glLineWidth((2.0f * U.pixelsize) - 1);
		glColor3ub(0x50, 0x50, 0x50);
		glBegin(GL_LINES);
		for (sa = win->screen->areabase.first; sa; sa = sa->next)
			drawscredge_area(sa, winsize_x, winsize_y);
		glEnd();
	}

	glLineWidth(1);
	glColor3ub(0, 0, 0);
	glBegin(GL_LINES);
	for (sa = win->screen->areabase.first; sa; sa = sa->next) {
		drawscredge_area(sa, winsize_x, winsize_y);

		/* gather area split/join info */
		if (sa->flag & AREA_FLAG_DRAWJOINFROM) sa1 = sa;
		if (sa->flag & AREA_FLAG_DRAWJOINTO) sa2 = sa;
		if (sa->flag & (AREA_FLAG_DRAWSPLIT_H | AREA_FLAG_DRAWSPLIT_V)) sa3 = sa;
	}
	glEnd();

	/* blended join arrow */
	if (sa1 && sa2) {
		int dir = area_getorientation(sa1, sa2);
		int dira = -1;
		if (dir != -1) {
			switch (dir) {
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
		glEnable(GL_BLEND);
		scrarea_draw_shape_dark(sa2, dir);
		scrarea_draw_shape_light(sa1, dira);
		glDisable(GL_BLEND);
	}
	
	/* splitpoint */
	if (sa3) {
		glEnable(GL_BLEND);
		glBegin(GL_LINES);
		glColor4ub(255, 255, 255, 100);
		
		if (sa3->flag & AREA_FLAG_DRAWSPLIT_H) {
			glVertex2s(sa3->totrct.xmin, win->eventstate->y);
			glVertex2s(sa3->totrct.xmax, win->eventstate->y);
			glColor4ub(0, 0, 0, 100);
			glVertex2s(sa3->totrct.xmin, win->eventstate->y + 1);
			glVertex2s(sa3->totrct.xmax, win->eventstate->y + 1);
		}
		else {
			glVertex2s(win->eventstate->x, sa3->totrct.ymin);
			glVertex2s(win->eventstate->x, sa3->totrct.ymax);
			glColor4ub(0, 0, 0, 100);
			glVertex2s(win->eventstate->x + 1, sa3->totrct.ymin);
			glVertex2s(win->eventstate->x + 1, sa3->totrct.ymax);
		}
		glEnd();
		glDisable(GL_BLEND);
	}
	
	win->screen->do_draw = false;
}

/* helper call for below, dpi changes headers */
static void screen_refresh_headersizes(void)
{
	const ListBase *lb = BKE_spacetypes_list();
	SpaceType *st;
	
	for (st = lb->first; st; st = st->next) {
		ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_HEADER);
		if (art) art->prefsizey = ED_area_headersize();
	}
}

/* make this screen usable */
/* for file read and first use, for scaling window, area moves */
void ED_screen_refresh(wmWindowManager *wm, wmWindow *win)
{	
	/* exception for bg mode, we only need the screen context */
	if (!G.background) {
		const int winsize_x = WM_window_pixels_x(win);
		const int winsize_y = WM_window_pixels_y(win);
		ScrArea *sa;
		rcti winrct;
	
		winrct.xmin = 0;
		winrct.xmax = winsize_x - 1;
		winrct.ymin = 0;
		winrct.ymax = winsize_y - 1;
		
		/* header size depends on DPI, let's verify */
		WM_window_set_dpi(win);
		screen_refresh_headersizes();
		
		screen_test_scale(win->screen, winsize_x, winsize_y);
		
		if (win->screen->mainwin == 0) {
			win->screen->mainwin = wm_subwindow_open(win, &winrct, false);
		}
		else {
			wm_subwindow_position(win, win->screen->mainwin, &winrct, false);
		}
		
		for (sa = win->screen->areabase.first; sa; sa = sa->next) {
			/* set spacetype and region callbacks, calls init() */
			/* sets subwindows for regions, adds handlers */
			ED_area_initialize(wm, win, sa);
		}
	
		/* wake up animtimer */
		if (win->screen->animtimer)
			WM_event_timer_sleep(wm, win, win->screen->animtimer, false);
	}

	if (G.debug & G_DEBUG_EVENTS) {
		printf("%s: set screen\n", __func__);
	}
	win->screen->do_refresh = false;

	win->screen->context = ed_screen_context;
}

/* file read, set all screens, ... */
void ED_screens_initialize(wmWindowManager *wm)
{
	wmWindow *win;
	
	for (win = wm->windows.first; win; win = win->next) {
		
		if (win->screen == NULL)
			win->screen = G.main->screen.first;
		
		ED_screen_refresh(wm, win);
	}
}


/* *********** exit calls are for closing running stuff ******** */

void ED_region_exit(bContext *C, ARegion *ar)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	ARegion *prevar = CTX_wm_region(C);

	if (ar->type && ar->type->exit)
		ar->type->exit(wm, ar);

	CTX_wm_region_set(C, ar);
	WM_event_remove_handlers(C, &ar->handlers);
	if (ar->swinid) {
		wm_subwindow_close(CTX_wm_window(C), ar->swinid);
		ar->swinid = 0;
	}
	
	if (ar->headerstr) {
		MEM_freeN(ar->headerstr);
		ar->headerstr = NULL;
	}
	
	if (ar->regiontimer) {
		WM_event_remove_timer(CTX_wm_manager(C), CTX_wm_window(C), ar->regiontimer);
		ar->regiontimer = NULL;
	}

	CTX_wm_region_set(C, prevar);
}

void ED_area_exit(bContext *C, ScrArea *sa)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	ScrArea *prevsa = CTX_wm_area(C);
	ARegion *ar;

	if (sa->type && sa->type->exit)
		sa->type->exit(wm, sa);

	CTX_wm_area_set(C, sa);
	for (ar = sa->regionbase.first; ar; ar = ar->next)
		ED_region_exit(C, ar);

	WM_event_remove_handlers(C, &sa->handlers);
	CTX_wm_area_set(C, prevsa);
}

void ED_screen_exit(bContext *C, wmWindow *window, bScreen *screen)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *prevwin = CTX_wm_window(C);
	ScrArea *sa;
	ARegion *ar;

	CTX_wm_window_set(C, window);
	
	if (screen->animtimer)
		WM_event_remove_timer(wm, window, screen->animtimer);
	screen->animtimer = NULL;
	screen->scrubbing = false;

	if (screen->mainwin)
		wm_subwindow_close(window, screen->mainwin);
	screen->mainwin = 0;
	screen->subwinactive = 0;
	
	for (ar = screen->regionbase.first; ar; ar = ar->next)
		ED_region_exit(C, ar);

	for (sa = screen->areabase.first; sa; sa = sa->next)
		ED_area_exit(C, sa);

	/* mark it available for use for other windows */
	screen->winid = 0;
	
	if (prevwin->screen->temp == 0) {
		/* use previous window if possible */
		CTX_wm_window_set(C, prevwin);
	}
	else {
		/* none otherwise */
		CTX_wm_window_set(C, NULL);
	}
	
}

/* *********************************** */

/* case when on area-edge or in azones, or outside window */
static void screen_cursor_set(wmWindow *win, wmEvent *event)
{
	const int winsize_x = WM_window_pixels_x(win);
	const int winsize_y = WM_window_pixels_y(win);

	AZone *az = NULL;
	ScrArea *sa;
	
	for (sa = win->screen->areabase.first; sa; sa = sa->next)
		if ((az = is_in_area_actionzone(sa, &event->x)))
			break;
	
	if (sa) {
		if (az->type == AZONE_AREA)
			WM_cursor_set(win, CURSOR_EDIT);
		else if (az->type == AZONE_REGION) {
			if (az->edge == AE_LEFT_TO_TOPRIGHT || az->edge == AE_RIGHT_TO_TOPLEFT)
				WM_cursor_set(win, CURSOR_X_MOVE);
			else
				WM_cursor_set(win, CURSOR_Y_MOVE);
		}
	}
	else {
		ScrEdge *actedge = screen_find_active_scredge(win->screen, winsize_x, winsize_y, event->x, event->y);
		
		if (actedge) {
			if (scredge_is_horizontal(actedge))
				WM_cursor_set(win, CURSOR_Y_MOVE);
			else
				WM_cursor_set(win, CURSOR_X_MOVE);
		}
		else
			WM_cursor_set(win, CURSOR_STD);
	}
}


/* called in wm_event_system.c. sets state vars in screen, cursors */
/* event type is mouse move */
void ED_screen_set_subwinactive(bContext *C, wmEvent *event)
{
	wmWindow *win = CTX_wm_window(C);
	
	if (win->screen) {
		bScreen *scr = win->screen;
		ScrArea *sa;
		ARegion *ar;
		int oldswin = scr->subwinactive;

		for (sa = scr->areabase.first; sa; sa = sa->next) {
			if (event->x > sa->totrct.xmin && event->x < sa->totrct.xmax)
				if (event->y > sa->totrct.ymin && event->y < sa->totrct.ymax)
					if (NULL == is_in_area_actionzone(sa, &event->x))
						break;
		}
		if (sa) {
			/* make overlap active when mouse over */
			for (ar = sa->regionbase.first; ar; ar = ar->next) {
				if (BLI_rcti_isect_pt_v(&ar->winrct, &event->x)) {
					scr->subwinactive = ar->swinid;
					break;
				}
			}
		}
		else
			scr->subwinactive = scr->mainwin;
		
		/* check for redraw headers */
		if (oldswin != scr->subwinactive) {

			for (sa = scr->areabase.first; sa; sa = sa->next) {
				bool do_draw = false;
				
				for (ar = sa->regionbase.first; ar; ar = ar->next)
					if (ar->swinid == oldswin || ar->swinid == scr->subwinactive)
						do_draw = true;
				
				if (do_draw) {
					for (ar = sa->regionbase.first; ar; ar = ar->next)
						if (ar->regiontype == RGN_TYPE_HEADER)
							ED_region_tag_redraw(ar);
				}
			}
		}
		
		/* cursors, for time being set always on edges, otherwise aregion doesnt switch */
		if (scr->subwinactive == scr->mainwin) {
			screen_cursor_set(win, event);
		}
		else {
			/* notifier invokes freeing the buttons... causing a bit too much redraws */
			if (oldswin != scr->subwinactive) {
				region_cursor_set(win, scr->subwinactive, true);

				/* this used to be a notifier, but needs to be done immediate
				 * because it can undo setting the right button as active due
				 * to delayed notifier handling */
				UI_screen_free_active_but(C, win->screen);
			}
			else
				region_cursor_set(win, scr->subwinactive, false);
		}
	}
}

int ED_screen_area_active(const bContext *C)
{
	wmWindow *win = CTX_wm_window(C);
	bScreen *sc = CTX_wm_screen(C);
	ScrArea *sa = CTX_wm_area(C);

	if (win && sc && sa) {
		AZone *az = is_in_area_actionzone(sa, &win->eventstate->x);
		ARegion *ar;
		
		if (az && az->type == AZONE_REGION)
			return 1;
		
		for (ar = sa->regionbase.first; ar; ar = ar->next)
			if (ar->swinid == sc->subwinactive)
				return 1;
	}
	return 0;
}

/**
 * operator call, WM + Window + screen already existed before
 *
 * \warning Do NOT call in area/region queues!
 * \returns success.
 */
bool ED_screen_set(bContext *C, bScreen *sc)
{
	Main *bmain = CTX_data_main(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	bScreen *oldscreen = CTX_wm_screen(C);
	
	/* validate screen, it's called with notifier reference */
	if (BLI_findindex(&bmain->screen, sc) == -1) {
		return true;
	}

	if (ELEM(sc->state, SCREENMAXIMIZED, SCREENFULL)) {
		/* find associated full */
		bScreen *sc1;
		for (sc1 = bmain->screen.first; sc1; sc1 = sc1->id.next) {
			ScrArea *sa = sc1->areabase.first;
			if (sa->full == sc) {
				sc = sc1;
				break;
			}
		}
	}

	/* check for valid winid */
	if (sc->winid != 0 && sc->winid != win->winid) {
		return false;
	}
	
	if (oldscreen != sc) {
		wmTimer *wt = oldscreen->animtimer;
		ScrArea *sa;
		Scene *oldscene = oldscreen->scene;

		/* remove handlers referencing areas in old screen */
		for (sa = oldscreen->areabase.first; sa; sa = sa->next) {
			WM_event_remove_area_handler(&win->modalhandlers, sa);
		}

		/* we put timer to sleep, so screen_exit has to think there's no timer */
		oldscreen->animtimer = NULL;
		if (wt) {
			WM_event_timer_sleep(wm, win, wt, true);
		}

		ED_screen_exit(C, win, oldscreen);

		/* Same scene, "transfer" playback to new screen. */
		if (wt) {
			if (oldscene == sc->scene) {
				sc->animtimer = wt;
			}
			/* Else, stop playback. */
			else {
				oldscreen->animtimer = wt;
				ED_screen_animation_play(C, 0, 0);
			}
		}

		win->screen = sc;
		CTX_wm_window_set(C, win);  // stores C->wm.screen... hrmf
		
		/* prevent multiwin errors */
		sc->winid = win->winid;
		
		ED_screen_refresh(CTX_wm_manager(C), CTX_wm_window(C));
		WM_event_add_notifier(C, NC_WINDOW, NULL);
		WM_event_add_notifier(C, NC_SCREEN | ND_SCREENSET, sc);
		
		/* makes button hilites work */
		WM_event_add_mousemove(C);

		/* Needed to make sure all the derivedMeshes are
		 * up-to-date before viewport starts acquiring this.
		 *
		 * This is needed in cases when, for example, boolean
		 * modifier uses operant from invisible layer.
		 * Without this trick boolean wouldn't apply correct.
		 *
		 * Quite the same happens when setting screen's scene,
		 * so perhaps this is in fact correct thing to do.
		 */
		if (oldscene != sc->scene) {
			BKE_scene_set_background(bmain, sc->scene);
		}

		/* Always do visible update since it's possible new screen will
		 * have different layers visible in 3D view-ports.
		 * This is possible because of view3d.lock_camera_and_layers option.
		 */
		DAG_on_visible_update(bmain, false);
	}

	return true;
}

static bool ed_screen_used(wmWindowManager *wm, bScreen *sc)
{
	wmWindow *win;

	for (win = wm->windows.first; win; win = win->next) {
		if (win->screen == sc) {
			return true;
		}

		if (ELEM(win->screen->state, SCREENMAXIMIZED, SCREENFULL)) {
			ScrArea *sa = win->screen->areabase.first;
			if (sa->full == sc) {
				return true;
			}
		}
	}

	return false;
}

/* only call outside of area/region loops */
bool ED_screen_delete(bContext *C, bScreen *sc)
{
	Main *bmain = CTX_data_main(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	bScreen *newsc;
	
	/* don't allow deleting temp fullscreens for now */
	if (ELEM(sc->state, SCREENMAXIMIZED, SCREENFULL)) {
		return false;
	}

	/* screen can only be in use by one window at a time, so as
	 * long as we are able to find a screen that is unused, we
	 * can safely assume ours is not in use anywhere an delete it */

	for (newsc = sc->id.prev; newsc; newsc = newsc->id.prev)
		if (!ed_screen_used(wm, newsc) && !newsc->temp)
			break;
	
	if (!newsc) {
		for (newsc = sc->id.next; newsc; newsc = newsc->id.next)
			if (!ed_screen_used(wm, newsc) && !newsc->temp)
				break;
	}

	if (!newsc) {
		return false;
	}

	ED_screen_set(C, newsc);

	if (win->screen != sc) {
		BKE_libblock_free(bmain, sc);
		return true;
	}
	else {
		return false;
	}
}

static void ed_screen_set_3dview_camera(Scene *scene, bScreen *sc, ScrArea *sa, View3D *v3d)
{
	/* fix any cameras that are used in the 3d view but not in the scene */
	BKE_screen_view3d_sync(v3d, scene);

	if (!v3d->camera || !BKE_scene_base_find(scene, v3d->camera)) {
		v3d->camera = BKE_scene_camera_find(sc->scene);
		// XXX if (sc == curscreen) handle_view3d_lock();
		if (!v3d->camera) {
			ARegion *ar;
			ListBase *regionbase;
			
			/* regionbase is in different place depending if space is active */
			if (v3d == sa->spacedata.first)
				regionbase = &sa->regionbase;
			else
				regionbase = &v3d->regionbase;
				
			for (ar = regionbase->first; ar; ar = ar->next) {
				if (ar->regiontype == RGN_TYPE_WINDOW) {
					RegionView3D *rv3d = ar->regiondata;
					if (rv3d->persp == RV3D_CAMOB) {
						rv3d->persp = RV3D_PERSP;
					}
				}
			}
		}
	}
}

/* only call outside of area/region loops */
void ED_screen_set_scene(bContext *C, bScreen *screen, Scene *scene)
{
	Main *bmain = CTX_data_main(C);
	bScreen *sc;

	if (screen == NULL)
		return;
	
	if (ed_screen_used(CTX_wm_manager(C), screen))
		ED_object_editmode_exit(C, EM_FREEDATA | EM_DO_UNDO);

	for (sc = bmain->screen.first; sc; sc = sc->id.next) {
		if ((U.flag & USER_SCENEGLOBAL) || sc == screen) {
			
			if (scene != sc->scene) {
				/* all areas endlocalview */
				// XXX	ScrArea *sa = sc->areabase.first;
				//	while (sa) {
				//		endlocalview(sa);
				//		sa = sa->next;
				//	}
				sc->scene = scene;
			}
			
		}
	}
	
	//  copy_view3d_lock(0);	/* space.c */
	
	/* are there cameras in the views that are not in the scene? */
	for (sc = bmain->screen.first; sc; sc = sc->id.next) {
		if ((U.flag & USER_SCENEGLOBAL) || sc == screen) {
			ScrArea *sa = sc->areabase.first;
			while (sa) {
				SpaceLink *sl = sa->spacedata.first;
				while (sl) {
					if (sl->spacetype == SPACE_VIEW3D) {
						View3D *v3d = (View3D *) sl;
						ed_screen_set_3dview_camera(scene, sc, sa, v3d);

					}
					sl = sl->next;
				}
				sa = sa->next;
			}
		}
	}
	
	CTX_data_scene_set(C, scene);
	BKE_scene_set_background(bmain, scene);
	DAG_on_visible_update(bmain, false);
	
	ED_render_engine_changed(bmain);
	ED_update_for_newframe(bmain, scene, 1);
	
	/* complete redraw */
	WM_event_add_notifier(C, NC_WINDOW, NULL);
	
}

/**
 * \note Only call outside of area/region loops
 * \return true if successful
 */
bool ED_screen_delete_scene(bContext *C, Scene *scene)
{
	Main *bmain = CTX_data_main(C);
	Scene *newscene;

	if (scene->id.prev)
		newscene = scene->id.prev;
	else if (scene->id.next)
		newscene = scene->id.next;
	else
		return false;

	ED_screen_set_scene(C, CTX_wm_screen(C), newscene);

	BKE_libblock_remap(bmain, scene, newscene, ID_REMAP_SKIP_INDIRECT_USAGE | ID_REMAP_SKIP_NEVER_NULL_USAGE);

	BKE_libblock_free_us(bmain, scene);

	return true;
}

ScrArea *ED_screen_full_newspace(bContext *C, ScrArea *sa, int type)
{
	wmWindow *win = CTX_wm_window(C);
	bScreen *screen = CTX_wm_screen(C);
	ScrArea *newsa = NULL;

	if (!sa || sa->full == NULL) {
		newsa = ED_screen_state_toggle(C, win, sa, SCREENMAXIMIZED);
	}
	
	if (!newsa) {
		if (sa->full && (screen->state == SCREENMAXIMIZED)) {
			/* if this has been called from the temporary info header generated in
			 * temp fullscreen layouts, find the correct fullscreen area to change
			 * to create a new space inside */
			for (newsa = screen->areabase.first; newsa; newsa = newsa->next) {
				if (!(sa->flag & AREA_TEMP_INFO))
					break;
			}
		}
		else {
			newsa = sa;
		}
	}

	BLI_assert(newsa);

	if (sa && (sa->spacetype != type)) {
		newsa->flag |= AREA_FLAG_TEMP_TYPE;
	}
	else {
		newsa->flag &= ~AREA_FLAG_TEMP_TYPE;
	}

	ED_area_newspace(C, newsa, type, (newsa->flag & AREA_FLAG_TEMP_TYPE));

	return newsa;
}

/**
 * \a was_prev_temp for the case previous space was a temporary fullscreen as well
 */
void ED_screen_full_prevspace(bContext *C, ScrArea *sa)
{
	BLI_assert(sa->full);

	if (sa->flag & AREA_FLAG_STACKED_FULLSCREEN) {
		/* stacked fullscreen -> only go back to previous screen and don't toggle out of fullscreen */
		ED_area_prevspace(C, sa);
	}
	else {
		ED_screen_restore_temp_type(C, sa);
	}
}

void ED_screen_restore_temp_type(bContext *C, ScrArea *sa)
{
	/* incase nether functions below run */
	ED_area_tag_redraw(sa);

	if (sa->flag & AREA_FLAG_TEMP_TYPE) {
		ED_area_prevspace(C, sa);
		sa->flag &= ~AREA_FLAG_TEMP_TYPE;
	}

	if (sa->full) {
		ED_screen_state_toggle(C, CTX_wm_window(C), sa, SCREENMAXIMIZED);
	}
}

/* restore a screen / area back to default operation, after temp fullscreen modes */
void ED_screen_full_restore(bContext *C, ScrArea *sa)
{
	wmWindow *win = CTX_wm_window(C);
	SpaceLink *sl = sa->spacedata.first;
	bScreen *screen = CTX_wm_screen(C);
	short state = (screen ? screen->state : SCREENMAXIMIZED);
	
	/* if fullscreen area has a temporary space (such as a file browser or fullscreen render
	 * overlaid on top of an existing setup) then return to the previous space */
	
	if (sl->next) {
		if (sa->flag & AREA_FLAG_TEMP_TYPE) {
			ED_screen_full_prevspace(C, sa);
		}
		else {
			ED_screen_state_toggle(C, win, sa, state);
		}
		/* warning: 'sa' may be freed */
	}
	/* otherwise just tile the area again */
	else {
		ED_screen_state_toggle(C, win, sa, state);
	}
}

/**
 * this function toggles: if area is maximized/full then the parent will be restored
 *
 * \warning \a sa may be freed.
 */
ScrArea *ED_screen_state_toggle(bContext *C, wmWindow *win, ScrArea *sa, const short state)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	bScreen *sc, *oldscreen;
	ARegion *ar;

	if (sa) {
		/* ensure we don't have a button active anymore, can crash when
		 * switching screens with tooltip open because region and tooltip
		 * are no longer in the same screen */
		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			UI_blocklist_free(C, &ar->uiblocks);

			if (ar->regiontimer) {
				WM_event_remove_timer(wm, NULL, ar->regiontimer);
				ar->regiontimer = NULL;
			}
		}

		/* prevent hanging header prints */
		ED_area_headerprint(sa, NULL);
	}

	if (sa && sa->full) {
		/* restoring back to SCREENNORMAL */
		ScrArea *old;

		sc = sa->full;       /* the old screen to restore */
		oldscreen = win->screen; /* the one disappearing */

		sc->state = SCREENNORMAL;

		/* find old area */
		for (old = sc->areabase.first; old; old = old->next)
			if (old->full) break;
		if (old == NULL) {
			if (G.debug & G_DEBUG)
				printf("%s: something wrong in areafullscreen\n", __func__);
			return NULL;
		}

		if (state == SCREENFULL) {
			/* restore the old side panels/header visibility */
			for (ar = sa->regionbase.first; ar; ar = ar->next) {
				ar->flag = ar->flagfullscreen;
			}
		}

		ED_area_data_swap(old, sa);
		if (sa->flag & AREA_TEMP_INFO) sa->flag &= ~AREA_TEMP_INFO;
		old->full = NULL;

		/* animtimer back */
		sc->animtimer = oldscreen->animtimer;
		oldscreen->animtimer = NULL;

		ED_screen_set(C, sc);

		BKE_libblock_free(CTX_data_main(C), oldscreen);

		/* After we've restored back to SCREENNORMAL, we have to wait with
		 * screen handling as it uses the area coords which aren't updated yet.
		 * Without doing so, the screen handling gets wrong area coords,
		 * which in worst case can lead to crashes (see T43139) */
		sc->skip_handling = true;
	}
	else {
		/* change from SCREENNORMAL to new state */
		ScrArea *newa;
		char newname[MAX_ID_NAME - 2];

		oldscreen = win->screen;

		oldscreen->state = state;
		BLI_snprintf(newname, sizeof(newname), "%s-%s", oldscreen->id.name + 2, "nonnormal");
		sc = ED_screen_add(win, oldscreen->scene, newname);
		sc->state = state;
		sc->redraws_flag = oldscreen->redraws_flag;
		sc->temp = oldscreen->temp;

		/* timer */
		sc->animtimer = oldscreen->animtimer;
		oldscreen->animtimer = NULL;

		/* use random area when we have no active one, e.g. when the
		 * mouse is outside of the window and we open a file browser */
		if (!sa)
			sa = oldscreen->areabase.first;

		if (state == SCREENMAXIMIZED) {
			/* returns the top small area */
			newa = area_split(sc, (ScrArea *)sc->areabase.first, 'h', 0.99f, 1);
			ED_area_newspace(C, newa, SPACE_INFO, false);

			/* copy area */
			newa = newa->prev;
			ED_area_data_swap(newa, sa);
			sa->flag |= AREA_TEMP_INFO;

			sa->full = oldscreen;
			newa->full = oldscreen;
			newa->next->full = oldscreen; // XXX
		}
		else if (state == SCREENFULL) {
			newa = (ScrArea *)sc->areabase.first;

			/* copy area */
			ED_area_data_swap(newa, sa);
			newa->flag = sa->flag; /* mostly for AREA_FLAG_WASFULLSCREEN */

			/* temporarily hide the side panels/header */
			for (ar = newa->regionbase.first; ar; ar = ar->next) {
				ar->flagfullscreen = ar->flag;

				if (ELEM(ar->regiontype,
				         RGN_TYPE_UI,
				         RGN_TYPE_HEADER,
				         RGN_TYPE_TOOLS))
				{
					ar->flag |= RGN_FLAG_HIDDEN;
				}
			}

			sa->full = oldscreen;
			newa->full = oldscreen;
		}
		else {
			BLI_assert(false);
		}

		ED_screen_set(C, sc);
	}

	/* XXX bad code: setscreen() ends with first area active. fullscreen render assumes this too */
	CTX_wm_area_set(C, sc->areabase.first);

	return sc->areabase.first;
}

/* update frame rate info for viewport drawing */
void ED_refresh_viewport_fps(bContext *C)
{
	wmTimer *animtimer = CTX_wm_screen(C)->animtimer;
	Scene *scene = CTX_data_scene(C);
	
	/* is anim playback running? */
	if (animtimer && (U.uiflag & USER_SHOW_FPS)) {
		ScreenFrameRateInfo *fpsi = scene->fps_info;
		
		/* if there isn't any info, init it first */
		if (fpsi == NULL)
			fpsi = scene->fps_info = MEM_callocN(sizeof(ScreenFrameRateInfo), "refresh_viewport_fps fps_info");
		
		/* update the values */
		fpsi->redrawtime = fpsi->lredrawtime;
		fpsi->lredrawtime = animtimer->ltime;
	}
	else {
		/* playback stopped or shouldn't be running */
		if (scene->fps_info)
			MEM_freeN(scene->fps_info);
		scene->fps_info = NULL;
	}
}

/* redraws: uses defines from stime->redraws 
 * enable: 1 - forward on, -1 - backwards on, 0 - off
 */
void ED_screen_animation_timer(bContext *C, int redraws, int refresh, int sync, int enable)
{
	bScreen *screen = CTX_wm_screen(C);
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *win = CTX_wm_window(C);
	Scene *scene = CTX_data_scene(C);
	bScreen *stopscreen = ED_screen_animation_playing(wm);
	
	if (stopscreen) {
		WM_event_remove_timer(wm, win, stopscreen->animtimer);
		stopscreen->animtimer = NULL;
	}
	
	if (enable) {
		ScreenAnimData *sad = MEM_callocN(sizeof(ScreenAnimData), "ScreenAnimData");
		
		screen->animtimer = WM_event_add_timer(wm, win, TIMER0, (1.0 / FPS));
		
		sad->ar = CTX_wm_region(C);
		/* if startframe is larger than current frame, we put currentframe on startframe.
		 * note: first frame then is not drawn! (ton) */
		if (PRVRANGEON) {
			if (scene->r.psfra > scene->r.cfra) {
				sad->sfra = scene->r.cfra;
				scene->r.cfra = scene->r.psfra;
			}
			else
				sad->sfra = scene->r.cfra;
		}
		else {
			if (scene->r.sfra > scene->r.cfra) {
				sad->sfra = scene->r.cfra;
				scene->r.cfra = scene->r.sfra;
			}
			else
				sad->sfra = scene->r.cfra;
		}
		sad->redraws = redraws;
		sad->refresh = refresh;
		sad->flag |= (enable < 0) ? ANIMPLAY_FLAG_REVERSE : 0;
		sad->flag |= (sync == 0) ? ANIMPLAY_FLAG_NO_SYNC : (sync == 1) ? ANIMPLAY_FLAG_SYNC : 0;

		ScrArea *sa = CTX_wm_area(C);

		char spacetype = -1;

		if (sa)
			spacetype = sa->spacetype;

		sad->from_anim_edit = (ELEM(spacetype, SPACE_IPO, SPACE_ACTION, SPACE_NLA, SPACE_TIME));

		screen->animtimer->customdata = sad;
		
	}

	/* notifier catched by top header, for button */
	WM_event_add_notifier(C, NC_SCREEN | ND_ANIMPLAY, NULL);
}

/* helper for screen_animation_play() - only to be used for TimeLine */
static ARegion *time_top_left_3dwindow(bScreen *screen)
{
	ARegion *aret = NULL;
	ScrArea *sa;
	int min = 10000;
	
	for (sa = screen->areabase.first; sa; sa = sa->next) {
		if (sa->spacetype == SPACE_VIEW3D) {
			ARegion *ar;
			for (ar = sa->regionbase.first; ar; ar = ar->next) {
				if (ar->regiontype == RGN_TYPE_WINDOW) {
					if (ar->winrct.xmin - ar->winrct.ymin < min) {
						aret = ar;
						min = ar->winrct.xmin - ar->winrct.ymin;
					}
				}
			}
		}
	}

	return aret;
}

void ED_screen_animation_timer_update(bScreen *screen, int redraws, int refresh)
{
	if (screen && screen->animtimer) {
		wmTimer *wt = screen->animtimer;
		ScreenAnimData *sad = wt->customdata;
		
		sad->redraws = redraws;
		sad->refresh = refresh;
		sad->ar = NULL;
		if (redraws & TIME_REGION)
			sad->ar = time_top_left_3dwindow(screen);
	}
}

/* results in fully updated anim system
 * screen can be NULL */
void ED_update_for_newframe(Main *bmain, Scene *scene, int UNUSED(mute))
{
	wmWindowManager *wm = bmain->wm.first;
	wmWindow *window;
	int layers = 0;

#ifdef DURIAN_CAMERA_SWITCH
	void *camera = BKE_scene_camera_switch_find(scene);
	if (camera && scene->camera != camera) {
		bScreen *sc;
		scene->camera = camera;
		/* are there cameras in the views that are not in the scene? */
		for (sc = bmain->screen.first; sc; sc = sc->id.next) {
			BKE_screen_view3d_scene_sync(sc);
		}
	}
#endif
	
	ED_clip_update_frame(bmain, scene->r.cfra);

	/* get layers from all windows */
	for (window = wm->windows.first; window; window = window->next)
		layers |= BKE_screen_visible_layers(window->screen, scene);

	/* this function applies the changes too */
	BKE_scene_update_for_newframe(bmain->eval_ctx, bmain, scene, layers);

	/* composite */
	if (scene->use_nodes && scene->nodetree)
		ntreeCompositTagAnimated(scene->nodetree);
	
	/* update animated texture nodes */
	{
		Tex *tex;
		for (tex = bmain->tex.first; tex; tex = tex->id.next) {
			if (tex->use_nodes && tex->nodetree) {
				ntreeTexTagAnimated(tex->nodetree);
			}
		}
	}
	
}

/*
 * return true if any active area requires to see in 3D
 */
bool ED_screen_stereo3d_required(bScreen *screen)
{
	ScrArea *sa;
	Scene *sce = screen->scene;
	const bool is_multiview = (sce->r.scemode & R_MULTIVIEW) != 0;

	for (sa = screen->areabase.first; sa; sa = sa->next) {
		switch (sa->spacetype) {
			case SPACE_VIEW3D:
			{
				View3D *v3d;

				if (!is_multiview)
					continue;

				v3d = sa->spacedata.first;
				if (v3d->camera && v3d->stereo3d_camera == STEREO_3D_ID) {
					ARegion *ar;
					for (ar = sa->regionbase.first; ar; ar = ar->next) {
						if (ar->regiondata && ar->regiontype == RGN_TYPE_WINDOW) {
							RegionView3D *rv3d = ar->regiondata;
							if (rv3d->persp == RV3D_CAMOB) {
								return true;
							}
						}
					}
				}
				break;
			}
			case SPACE_IMAGE:
			{
				SpaceImage *sima;

				/* images should always show in stereo, even if
				 * the file doesn't have views enabled */
				sima = sa->spacedata.first;
				if (sima->image && BKE_image_is_stereo(sima->image) &&
				    (sima->iuser.flag & IMA_SHOW_STEREO))
				{
					return true;
				}
				break;
			}
			case SPACE_NODE:
			{
				SpaceNode *snode;

				if (!is_multiview)
					continue;

				snode = sa->spacedata.first;
				if ((snode->flag & SNODE_BACKDRAW) && ED_node_is_compositor(snode)) {
					return true;
				}
				break;
			}
			case SPACE_SEQ:
			{
				SpaceSeq *sseq;

				if (!is_multiview)
					continue;

				sseq = sa->spacedata.first;
				if (ELEM(sseq->view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW)) {
					return true;
				}

				if (sseq->draw_flag & SEQ_DRAW_BACKDROP) {
					return true;
				}

				break;
			}
		}
	}

	return false;
}
