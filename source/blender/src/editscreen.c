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
 * All screen functions that are related to the interface
 * handling and drawing. Might be split up as well later...
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "nla.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>  /* isprint */
#include <stdio.h>
#include <math.h>

#include "GHOST_Types.h"

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_action_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_sound_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BLO_writefile.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_blender.h"
#include "BKE_screen.h"

#ifdef WITH_VERSE
#include "BKE_verse.h"
#endif

#include "BIF_cursors.h"
#include "BIF_drawscene.h"
#include "BIF_editsound.h"
#include "BIF_glutil.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_interface_icons.h"
#include "BIF_mainqueue.h"
#include "BIF_mywindow.h"
#include "BIF_previewrender.h"
#include "BIF_renderwin.h"
#include "BIF_retopo.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toets.h"
#include "BIF_toolbox.h"
#include "BIF_usiblender.h"
#include "BIF_keyval.h"
#include "BIF_resources.h"

#include "BSE_edit.h"
#include "BSE_filesel.h"
#include "BSE_headerbuttons.h"
#include "BSE_seqaudio.h"
#include "BSE_view.h"

#include "BPY_extern.h"
#include "mydevice.h"
#include "blendef.h"

#include "winlay.h"

/* TIPS:
 * 
 * - WATCH THE EDGES,  VERTICES HAVE TO BE IN ORDER...
	 (lowest pointer first). Otherwise unpredictable effects!
 * - problem: flags here are not nicely implemented. After usage
	 always reset to zero.
 */

/* comment added to test orange branch */

static void testareas(void);
static void area_autoplayscreen(void);
static void wait_for_event(void);


/* ********* Globals *********** */

static Window *mainwin= NULL;
static int prefsizx= 0, prefsizy= 0, prefstax= 0, prefstay= 0, start_maximized= 1;
static short dodrawscreen= 1;
static ScrArea *areawinar[MAXWIN];
static ScrArea *g_activearea= NULL;
short winqueue_break= 0;
ScrArea *curarea= 0;

/* prototypes -------------------*/
int afterqtest(short win, unsigned short evt);
unsigned short screen_qread(short *val, char *ascii);
void add_to_mainqueue(Window *win, void *user_data, short evt, short val, char ascii);
static void drawscredge_area(ScrArea *sa);

/**********************************************************************/

extern int textediting;

static void screen_set_cursor(bScreen *sc) 
{
	if (sc->winakt>3) {
		ScrArea *sa= areawinar[sc->winakt];

		set_cursor(sa->cursor);
	} else {
		set_cursor(CURSOR_STD);
	}
}

void waitcursor(int val)
{
	if(G.curscreen) {
		if(val) {
			set_cursor(CURSOR_WAIT);
		} else {
			screen_set_cursor(G.curscreen);
		}
	}
}

static int choose_cursor(ScrArea *sa)
{
	if (sa->spacetype==SPACE_VIEW3D) {
		if(G.obedit) return CURSOR_EDIT;
		else if(G.f & (G_VERTEXPAINT|G_WEIGHTPAINT|G_TEXTUREPAINT))
				return CURSOR_VPAINT;
		else if(FACESEL_PAINT_TEST) return CURSOR_FACESEL;
		else if(G.f & G_SCULPTMODE) return CURSOR_EDIT;
		else if(G.f & G_PARTICLEEDIT) return CURSOR_EDIT;
		else return CURSOR_STD;
	}
	else if (sa->spacetype==SPACE_TEXT) {
		return CURSOR_TEXTEDIT;
	}
	else {
		return CURSOR_STD;
	}
}

void wich_cursor(ScrArea *sa)
{
	sa->cursor= choose_cursor(sa);
	
	/* well... the waitcursor() is not a state, so this call will cancel it out */
	if(get_cursor()!=CURSOR_WAIT)
		screen_set_cursor(G.curscreen);
}


void setcursor_space(int spacetype, short cur)
{
	bScreen *sc;
	ScrArea *sa;

	for (sc= G.main->screen.first; sc; sc= sc->id.next)
		for (sa= sc->areabase.first; sa; sa= sa->next)
			if(sa->spacetype==spacetype)
				sa->cursor= cur;

	screen_set_cursor(G.curscreen);
}


/* *********  IN/OUT  ************* */

void getmouseco_sc(short *mval)		/* screen coordinates */
{
	getmouse(mval);
}

/* mouse_cursor called during a script (via Window.QHandle) need
 * this function for getmouseco_areawin to work: */
void set_g_activearea(ScrArea *sa)
{
	if (sa) g_activearea = sa;
}

void getmouseco_areawin(short *mval)		/* internal area coordinates */
{
	getmouseco_sc(mval);
	
	if(g_activearea && g_activearea->win) {
		mval[0]-= g_activearea->winrct.xmin;
		mval[1]-= g_activearea->winrct.ymin;
	}
}

void getmouseco_headwin(short *mval)		/* internal area coordinates */
{
	getmouseco_sc(mval);
	
	if(g_activearea && g_activearea->headwin) {
		mval[0]-= g_activearea->headrct.xmin;
		mval[1]-= g_activearea->headrct.ymin;
	}
}

void headerprint(char *str)
{
	if(curarea->headertype) {
		areawinset(curarea->headwin);
		
		headerbox(curarea);
		
		BIF_ThemeColor(TH_MENU_TEXT); /* better than cpack(0x0) color no? (desoto) */
		glRasterPos2i(20+curarea->headbutofs,  6);
		BMF_DrawString(G.font, str);
		
		curarea->head_swap= WIN_BACK_OK;
		areawinset(curarea->win);
	}
	else {
		// dunno... thats for later (ton)
	}
}

/* *********** STUFF ************** */

static int scredge_is_horizontal(ScrEdge *se)
{
	return (se->v1->vec.y == se->v2->vec.y);
}

static ScrEdge *screen_find_active_scredge(bScreen *sc, short *mval)
{
	ScrEdge *se;
	
	for (se= sc->edgebase.first; se; se= se->next) {
		if (scredge_is_horizontal(se)) {
			short min, max;
			min= MIN2(se->v1->vec.x, se->v2->vec.x);
			max= MAX2(se->v1->vec.x, se->v2->vec.x);
			
			if (abs(mval[1]-se->v1->vec.y)<=2 && mval[0] >= min && mval[0]<=max)
				return se;
		} 
		else {
			short min, max;
			min= MIN2(se->v1->vec.y, se->v2->vec.y);
			max= MAX2(se->v1->vec.y, se->v2->vec.y);

			if (abs(mval[0]-se->v1->vec.x)<=2 && mval[1] >= min && mval[1]<=max)
				return se;
		}
	}
	
	return NULL;
}

void areawinset(short win)
{
	if(win>3) {
		curarea= areawinar[win];
		if(curarea==0) {
			printf("error in areawinar %d ,areawinset\n", win);
			return;
		}
		
		BIF_SetTheme(curarea);
		
		switch(curarea->spacetype) {
		case SPACE_VIEW3D:
			G.vd= curarea->spacedata.first;
			break;
		case SPACE_IPO:
			G.sipo= curarea->spacedata.first;
			G.v2d= &G.sipo->v2d;
			break;
		case SPACE_BUTS:
			G.buts= curarea->spacedata.first;
			G.v2d= &G.buts->v2d;
			break;
		case SPACE_SEQ: {
			SpaceSeq *sseq= curarea->spacedata.first;
			G.v2d= &sseq->v2d;
			break;
		}
		case SPACE_OOPS:
			G.soops= curarea->spacedata.first;
			G.v2d= &G.soops->v2d;
			break;
		case SPACE_IMAGE:
			G.sima= curarea->spacedata.first;
			G.v2d= &G.sima->v2d;
			break;
		case SPACE_SOUND:
			G.ssound= curarea->spacedata.first;
			G.v2d= &G.ssound->v2d;
			break;
		case SPACE_ACTION:
			G.saction= curarea->spacedata.first;
			G.v2d= &G.saction->v2d;
			break;
		case SPACE_NLA:
			G.snla= curarea->spacedata.first;
			G.v2d= &G.snla->v2d;
			break;
		case SPACE_TIME:
		{
			SpaceTime *stime= curarea->spacedata.first;
			G.v2d= &stime->v2d;
		}
			break;
		case SPACE_NODE:
		{
			SpaceNode *snode= curarea->spacedata.first;
			G.v2d= &snode->v2d;
		}
			break;
		case SPACE_IMASEL:
		{
			SpaceImaSel *simasel= curarea->spacedata.first;
			G.v2d= &simasel->v2d;
		}
		default:
			break;
		}
	}
	
	if(win) mywinset(win);
}

#define SCR_BACK 0.55
#define SCR_ROUND 12

void headerbox(ScrArea *area)
{
	float width= area->winx;
	int active=0;

	glClearColor(SCR_BACK, SCR_BACK, SCR_BACK, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	active= area_is_active_area(area);

	if(active) BIF_ThemeColor(TH_HEADER);
	else BIF_ThemeColor(TH_HEADERDESEL);

	/* weird values here... is because of window matrix that centers buttons */
	if(area->headertype==HEADERTOP) {
		uiSetRoundBox(3);
		uiRoundBoxEmboss(-0.5+area->headbutofs, -10.0, width-1.5+area->headbutofs, HEADERY-2.0, SCR_ROUND, active);
	}
	else {
		uiSetRoundBox(12);
		uiRoundBoxEmboss(-0.5+area->headbutofs, -3.5, width-1.5+area->headbutofs, HEADERY+10, SCR_ROUND, active);
	}
	
	uiSetRoundBox(15);
}

int area_is_active_area(ScrArea *area)
{
	return (g_activearea && area==g_activearea);
}

void scrarea_do_headdraw(ScrArea *area)
{
	if (area->headertype) {
		areawinset(area->headwin);
	
		headerbox(area);
		
		/* we make scissor test slightly smaller not to destroy rounded headers */
		glScissor(area->headrct.xmin+5, area->headrct.ymin, area->winx-10, HEADERY);
		
		switch(area->spacetype) {
		case SPACE_FILE:	file_buttons();		break;
		case SPACE_INFO:	info_buttons();		break;
		case SPACE_VIEW3D:	view3d_buttons();	break;
		case SPACE_IPO:		ipo_buttons();		break;
		case SPACE_BUTS:	buts_buttons();		break;
		case SPACE_SEQ:		seq_buttons();		break;
		case SPACE_IMAGE:	image_buttons();	break;
		case SPACE_IMASEL:	imasel_buttons();	break;
		case SPACE_OOPS:	oops_buttons();		break;
		case SPACE_TEXT:	text_buttons();		break;
		case SPACE_SCRIPT:script_buttons();		break;
		case SPACE_SOUND:	sound_buttons();	break;
		case SPACE_ACTION:	action_buttons();	break;
		case SPACE_NLA:		nla_buttons();		break;
		case SPACE_TIME:	time_buttons(area);	break;
		case SPACE_NODE:	node_buttons(area);	break;
		}
		uiClearButLock();

		//glScissor(area->winrct.xmin, area->winrct.xmax, area->winx, area->winy);
		area->head_swap= WIN_BACK_OK;
	}
}
void scrarea_do_headchange(ScrArea *area)
{
	float ofs= area->headbutofs;

	if (area->headertype==HEADERDOWN) {
		bwin_ortho2(area->headwin, -0.375+ofs, area->headrct.xmax-area->headrct.xmin-0.375+ofs, -3.375, area->headrct.ymax-area->headrct.ymin-3.375+1.0);
	} else if (area->headertype==HEADERTOP) {
		bwin_ortho2(area->headwin, -0.375+ofs, area->headrct.xmax-area->headrct.xmin-0.375+ofs, -2.375-1.0, area->headrct.ymax-area->headrct.ymin-2.375);
	}
}


static void openheadwin(ScrArea *sa);
static void closeheadwin(ScrArea *sa);

static void scrarea_change_headertype(ScrArea *sa, int newtype) 
{
	sa->headertype= newtype;

	if (!newtype) {
		if (sa->headwin) {
			uiFreeBlocksWin(&sa->uiblocks, sa->headwin);
			closeheadwin(sa);
		}
	} else {
		if (!sa->headwin) {
			openheadwin(sa);
		}
	}

	testareas();
	mainqenter(DRAWEDGES, 1);
	winqueue_break= 1;
}

static void headmenu(ScrArea *sa)
{
	short val= pupmenu("Header %t|Top%x2|Bottom %x1|No Header %x0");
	
	if(val> -1) {
		scrarea_change_headertype(sa, val);
	}
}

static void addqueue_ext(short win, unsigned short event, short val, char ascii)
{
	if (win<4 || !areawinar[win]) {
		/* other win ids are for mainwin & renderwin */
	} 
	else {
		BWinEvent evt;

		memset(&evt, 0, sizeof(evt));
		evt.event= event;
		evt.val= val;
		evt.ascii= ascii;
		
		bwin_qadd(win, &evt);
	}
}

void addqueue(short win, unsigned short event, short val)
{
	addqueue_ext(win, event, val, 0);
}

void scrarea_queue_winredraw(ScrArea *area)
{
	addqueue(area->win, REDRAW, 1);
}
void scrarea_queue_headredraw(ScrArea *area)
{
	if (area->headwin) addqueue(area->headwin, REDRAW, 1);
}
void scrarea_queue_redraw(ScrArea *area)
{
	scrarea_queue_winredraw(area);
	scrarea_queue_headredraw(area);
}

static void scrollheader(ScrArea *area);
static void scrarea_dispatch_header_events(ScrArea *sa)
{
	ScrArea *tempsa;
	BWinEvent evt;
	short do_redraw=0, do_change=0;
	
	areawinset(sa->headwin);
	
	while(bwin_qread(sa->headwin, &evt)) {
		if(evt.val) {
			if( uiDoBlocks(&curarea->uiblocks, evt.event, 1)!=UI_NOTHING ) evt.event= 0;

			switch(evt.event) {
			case UI_BUT_EVENT:
				do_headerbuttons(evt.val);
				break;
			
			case LEFTMOUSE:
				if (G.qual & LR_CTRLKEY) {
					window_lower(mainwin);
				} else {
					window_raise(mainwin);
				}
				break;

			case MIDDLEMOUSE:
				scrollheader(sa);
				break;
			case RIGHTMOUSE:
				headmenu(sa);
				break;
			case REDRAW:
				do_redraw= 1;
				break;
			case CHANGED:
				sa->head_swap= 0;
				do_change= 1;
				do_redraw= 1;
				break;
			default:
				if (winqueue_break == 0) {
					scrarea_do_winhandle(sa, &evt);
					if (winqueue_break == 0) areawinset(sa->headwin);
				}
			}
			
			if(winqueue_break) return;
		}
	}

	/* test: does window still exist? */	
	tempsa= areawinar[sa->headwin];
	if(tempsa==0) return;
	
	/* this functional separation does not work as well as i expected... */
	if(do_change) scrarea_do_headchange(sa);
	if(do_redraw) scrarea_do_headdraw(sa);
}

static void scrarea_dispatch_events(ScrArea *sa)
{
	ScrArea *tempsa;
	BWinEvent evt;
	short do_redraw=0, do_change=0;
	
	if(sa!=curarea || sa->win!=mywinget()) areawinset(sa->win);

	while(bwin_qread(sa->win, &evt)) {
		if(evt.event==REDRAW) {
			do_redraw= 1;
		}
		else if(evt.event==CHANGED) {
			sa->win_swap= 0;
			do_change= 1;
			do_redraw= 1;
		}
		else {
			scrarea_do_winhandle(sa, &evt);
		}
		
		if(winqueue_break) return;
	}

	/* test: does window still exist */	
	tempsa= areawinar[sa->win];
	if(tempsa==0) return;

	if (do_change || do_redraw) {
		areawinset(sa->win);
		if(do_change)
			scrarea_do_winchange(curarea);
		if(do_redraw)
			scrarea_do_windraw(curarea);
	}
}

/***/


void markdirty_all()
{
	ScrArea *sa;
	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa->win) {
			scrarea_queue_winredraw(sa);
			sa->win_swap &= ~WIN_FRONT_OK;
		}
		if(sa->headwin) {
			scrarea_queue_headredraw(sa);
			sa->head_swap &= ~WIN_FRONT_OK;
		}
	}
}

/* but no redraw! */
void markdirty_all_back(void)
{
	ScrArea *sa;
	
	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa->win) {
			sa->win_swap &= ~WIN_BACK_OK;
		}
		if(sa->headwin) {
			sa->head_swap &= ~WIN_BACK_OK;
		}
	}
	
	/* if needed; backbuffer selection redraw */
	if(G.vd) G.vd->flag |= V3D_NEEDBACKBUFDRAW;

}

void markdirty_win_back(short winid)
{
	ScrArea *sa= areawinar[winid];
	if(sa) {
		if(sa->win==winid) sa->win_swap &= ~WIN_BACK_OK;
		else sa->head_swap &= ~WIN_BACK_OK;
	}
}


int is_allowed_to_change_screen(bScreen *new)
{
	/* not when curscreen is full
	 * not when obedit && old->scene!=new->scene
	 */
	
	if(new==0) return 0;
	if(G.curscreen->full != SCREENNORMAL) return 0;
	if(curarea->full) return 0;
	if(G.obedit) {
		if(G.curscreen->scene!=new->scene) return 0;
	}
	return 1;
}

void splash(void *data, int datasize, char *string)
{
	ImBuf *bbuf;
	int oldwin;
	short val;

	bbuf= IMB_ibImageFromMemory((int *)data, datasize, IB_rect);

	if (bbuf) {
		oldwin = mywinget();
		mywinset(G.curscreen->mainwin);
		
		if (string) {
			int x, y, maxy;
			unsigned int *rect;
			
			rect = bbuf->rect;
			maxy = MIN2(bbuf->y, 18);

			for (y = 0; y < maxy; y++) {
				for (x = 0; x < bbuf->x; x++) {
					*rect = 0xffffffff;
					rect++;
				}
			}
		}
		glDrawBuffer(GL_FRONT);
		
		uiDrawBoxShadow(200, (prefsizx-bbuf->x)/2, (prefsizy-bbuf->y)/2, (prefsizx+bbuf->x)/2, (prefsizy+bbuf->y)/2);
		
		glRasterPos2i((prefsizx-bbuf->x)/2, (prefsizy-bbuf->y)/2);	
		glDrawPixels(bbuf->x, bbuf->y, GL_RGBA, GL_UNSIGNED_BYTE, bbuf->rect);

		if (string) {
			void *font;
			int width;			
			
			if (BMF_GetStringWidth(font= G.font, string) > bbuf->x)
				if (BMF_GetStringWidth(font= G.fonts, string) > bbuf->x)
					font= G.fontss;
			
			width= BMF_GetStringWidth(font, string);
						
			glColor3ub(0, 0, 0);
			glRasterPos2i((prefsizx-width)/2, (prefsizy-bbuf->y)/2 + 6);
			BMF_DrawString(font, string);
		}

		bglFlush();
		glDrawBuffer(GL_BACK);
		
		IMB_freeImBuf(bbuf);
		
		// flush input buffers ....
		// this might break some things

		while (get_mbut()) {
			BIF_wait_for_statechange();
		}
		while(qtest()) {
			extern_qread(&val);
		}

		wait_for_event();
		
		mywinset(oldwin);
		markdirty_all();
		mainqenter(DRAWEDGES, 1);
	}
}

static void moveareas(ScrEdge *edge);
static void joinarea_interactive(ScrArea *area, ScrEdge *onedge);
static void splitarea_interactive(ScrArea *area, ScrEdge *onedge);
static ScrArea *test_edge_area(ScrArea *sa, ScrEdge *se);
static ScrEdge *screen_findedge(bScreen *sc, ScrVert *v1, ScrVert *v2);

static int isjoinable(ScrArea *area, ScrEdge *onedge)
{
	struct ScrArea *sa1 = area, *sa2;
	struct ScrEdge *se;
	
	sa1 = test_edge_area(sa1, onedge);
	if(sa1==0) return 0;

	/* find directions with same edge */
	sa2= G.curscreen->areabase.first;
	while(sa2) {
		if(sa2 != sa1) {
			se= screen_findedge(G.curscreen, sa2->v1, sa2->v2);
			if(onedge==se) return 1;
			se= screen_findedge(G.curscreen, sa2->v2, sa2->v3);
			if(onedge==se) return 1;
			se= screen_findedge(G.curscreen, sa2->v3, sa2->v4);
			if(onedge==se) return 1;
			se= screen_findedge(G.curscreen, sa2->v4, sa2->v1);
			if(onedge==se) return 1;
		}
		sa2= sa2->next;
	}
	
	return 0;		
}

static void screen_edge_edit_event(ScrArea *actarea, ScrEdge *actedge, short evt, short val) 
{
	if (val) {
			// don't allow users to edit full screens
		if (actarea && actarea->full)
			return;
	
		if (evt==LEFTMOUSE) {
			moveareas(actedge);
		} else if (evt==MIDDLEMOUSE || evt==RIGHTMOUSE) {
			int edgeop;
			char str[64] = "Split Area%x1|";
			
			if(isjoinable(actarea, actedge))	strcat(str, "Join Areas%x2|");
			if (actarea->headertype)			strcat(str, "No Header%x3");
			else								strcat(str, "Add Header%x3");
			
			edgeop= pupmenu(str);
			if      (edgeop==1)	splitarea_interactive(actarea, actedge);
			else if (edgeop==2)	joinarea_interactive(actarea, actedge);
			else if (edgeop==3)	scrarea_change_headertype(actarea,
										actarea->headertype?0:HEADERDOWN);
		}
		else blenderqread(evt, val);	// global hotkeys
	}
}

/***/

extern void mywindow_init_mainwin(Window *win, int orx, int ory, int sizex, int sizey);
void test_scale_screen(bScreen *);

static void resize_screens(int x, int y, int w, int h) {
	prefstax= x;
	prefstay= y;
	prefsizx= w;
	prefsizy= h;

	test_scale_screen(G.curscreen);
	testareas();
}

static void init_mainwin(void)
{
	int orx, ory, sizex, sizey;
	
	glEnable(GL_SCISSOR_TEST);

	window_get_position(mainwin, &orx, &ory);
	window_get_size(mainwin, &sizex, &sizey);

		/* XXX, temporary stupid fix for minimize at windows */
	if (!sizex && !sizey) {
		return;
	}

	mywindow_init_mainwin(mainwin, orx, ory, sizex, sizey);
	resize_screens(orx, ory, sizex, sizey);
}

/***/

static short afterqueue[MAXQUEUE][3];
static int nafterqitems= 0;

void addafterqueue(short win, unsigned short evt, short val)
{
	if (nafterqitems<MAXQUEUE) {
		int a;
		
		/* only one afterqueue event of each type */
		for(a=0; a<nafterqitems; a++) {
			if(afterqueue[a][0]==win && afterqueue[a][1]==evt) {
				afterqueue[a][2]= val;
				return;
			}
		}
		afterqueue[nafterqitems][0]= win;
		afterqueue[nafterqitems][1]= evt;
		afterqueue[nafterqitems][2]= val;
		nafterqitems++;
	}
}

static void append_afterqueue(void)
{
	while (nafterqitems) {
		short win= afterqueue[nafterqitems-1][0];
		unsigned short evt= afterqueue[nafterqitems-1][1];
		short val= afterqueue[nafterqitems-1][2];

		addqueue(win, evt, val);
		
		nafterqitems--;
	}
}

/* check for event in afterqueue, used in force_draw in space.c */
int afterqtest(short win, unsigned short evt)
{
	int a;
	
	for(a=0; a<nafterqitems; a++) {
		if(afterqueue[a][0]==win && afterqueue[a][1]==evt) return 1;
	}
	return 0;
}


static char ext_load_str[256]= {0, 0};
void add_readfile_event(char *filename)
{	
	mainqenter(LOAD_FILE, 1);
	strcpy(ext_load_str, filename);
	BLI_convertstringcode(ext_load_str, G.sce, G.scene->r.cfra);
}

static short ext_reshape= 0, ext_redraw=0, ext_inputchange=0, ext_mousemove=0, ext_undopush=0;

static void flush_extqd_events(void) {
	if (ext_inputchange) {
		mainqenter(INPUTCHANGE, ext_inputchange);
	} else if (ext_reshape) {
		mainqenter(RESHAPE, ext_redraw);
	} else if (ext_redraw) {
		mainqenter(REDRAW, ext_redraw);
	} else if (ext_undopush) {
		mainqenter(UNDOPUSH, ext_undopush);
	} else if (ext_mousemove) {
		short mouse[2];
		
		getmouseco_sc(mouse);
		
		mainqenter(MOUSEX, mouse[0]);
		mainqenter(MOUSEY, mouse[1]);
	}
	
	ext_inputchange= ext_reshape= ext_redraw= ext_mousemove= ext_undopush= 0;
}

int qtest(void)
{
	if (!mainqtest()) {
		winlay_process_events(0);
	}
	
	return mainqtest();
}

	/* return true if events are waiting anywhere */
int anyqtest(void)
{
	ScrArea *sa;

	if (nafterqitems || qtest()) return 1;

	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		if (bwin_qtest(sa->win)) return 1;
		if (sa->headwin && bwin_qtest(sa->headwin)) return 1;
	}

	return 0;
}

static void wait_for_event(void)
{
	while (!mainqtest()) {
		winlay_process_events(1);
	}
}

unsigned short screen_qread(short *val, char *ascii)
{
	unsigned short event;

	wait_for_event();

	event= mainqread(val, ascii);
	
	if(event==RIGHTSHIFTKEY || event==LEFTSHIFTKEY) {
		if(*val) G.qual |= LR_SHIFTKEY;
		else G.qual &= ~LR_SHIFTKEY;
	}
	else if(event==RIGHTALTKEY || event==LEFTALTKEY) {
		if(*val) G.qual |= LR_ALTKEY;
		else G.qual &= ~LR_ALTKEY;
	}
	else if(event==RIGHTCTRLKEY || event==LEFTCTRLKEY) {
		if(*val) G.qual |= LR_CTRLKEY;
		else G.qual &= ~LR_CTRLKEY;
	}
	else if(event==COMMANDKEY) {		// OSX
		if(*val) G.qual |= LR_COMMANDKEY;
		else G.qual &= ~LR_COMMANDKEY;
	}

	return event;
}

unsigned short extern_qread_ext(short *val, char *ascii)
{
	/* stores last INPUTCHANGE and last REDRAW */
	unsigned short event;
	
	event= screen_qread(val, ascii);
	if(event==RESHAPE) ext_reshape= *val;
	else if(event==REDRAW) ext_redraw= *val;
	else if(event==UNDOPUSH) ext_undopush= *val;
	else if(event==INPUTCHANGE) ext_inputchange= *val;
	else if(event==MOUSEY || event==MOUSEX) ext_mousemove= 1;
	else if((G.qual & (LR_CTRLKEY|LR_ALTKEY)) && event==F3KEY) {
		if(*val) {
			BIF_screendump(0);
			return ESCKEY;	/* go out of menu, if that was set */
		}
	}

	return event;
}
unsigned short extern_qread(short *val)
{
	char ascii;
	return extern_qread_ext(val, &ascii);
}

int blender_test_break(void)
{
	if (!G.background) {
		static double ltime= 0;
		double curtime= PIL_check_seconds_timer();

			/* only check for breaks every 10 milliseconds
			 * if we get called more often.
			 */
		if ((curtime-ltime)>.001) {
			ltime= curtime;

			while(qtest()) {
				short val;
				if (extern_qread(&val) == ESCKEY) {
					G.afbreek= 1;
				}
			}
		}
	}

	return (G.afbreek==1);
}

void reset_autosave(void) {
	window_set_timer(mainwin, U.savetime*60*1000, AUTOSAVE_FILE);
}

/* ************ handlers ************** */

/* don't know yet how the handlers will evolve, for simplicity
i choose for an array with eventcodes, this saves in a file!
*/
void add_screenhandler(bScreen *sc, short eventcode, short val)
{
	short a;
	
	// find empty spot
	for(a=0; a<SCREEN_MAXHANDLER; a+=2) {
		if( sc->handler[a]==eventcode ) {
			sc->handler[a+1]= val;
			break;
		}
		else if( sc->handler[a]==0) {
			sc->handler[a]= eventcode;
			sc->handler[a+1]= val;
			break;
		}
	}
	if(a==SCREEN_MAXHANDLER) printf("error; max (4) screen handlers reached!\n");
}

void rem_screenhandler(bScreen *sc, short eventcode)
{
	short a;
	
	for(a=0; a<SCREEN_MAXHANDLER; a+=2) {
		if( sc->handler[a]==eventcode) {
			sc->handler[a]= 0;
			break;
		}
	}
}

int has_screenhandler(bScreen *sc, short eventcode)
{
	short a;
	
	for(a=0; a<SCREEN_MAXHANDLER; a+=2) {
		if( sc->handler[a]==eventcode) {
			return 1;
		}
	}
	return 0;
}

static void animated_screen(bScreen *sc, short val)
{
	if ((val & TIME_WITH_SEQ_AUDIO)) {
		if(CFRA>=PEFRA) {
			CFRA= PSFRA;
			audiostream_stop();
			audiostream_start( CFRA );
		}
		else {
			int cfra = audiostream_pos();
			if(cfra <= CFRA) CFRA++;
			else CFRA= cfra;
		}
	}
	else {
		CFRA++;
		if(CFRA > PEFRA) CFRA= PSFRA;
	}
	
	update_for_newframe_nodraw(1);
	
	if(val & TIME_ALL_3D_WIN)
		allqueue(REDRAWVIEW3D, 0);
	else if(val & TIME_LEFTMOST_3D_WIN) {
		ScrArea *sa= sc->areabase.first, *samin=NULL;
		int min= 10000;
		for(; sa; sa= sa->next) {
			if(sa->spacetype==SPACE_VIEW3D) {
				if(sa->winrct.xmin - sa->winrct.ymin < min) {
					samin= sa;
					min= sa->winrct.xmin - sa->winrct.ymin;
				}
			}
		}
		if(samin) scrarea_queue_winredraw(samin);
	}
	if(val & TIME_ALL_ANIM_WIN) allqueue(REDRAWANIM, 0);
	if(val & TIME_ALL_BUTS_WIN) allqueue(REDRAWBUTSALL, 0);
	if(val & TIME_SEQ) {
		allqueue(REDRAWSEQ, 0);
	}
	
	allqueue(REDRAWTIME, 0);
}

/* because we still have to cope with subloops, this function is called
in viewmove() for example too */

/* returns 1 if something was handled */
/* restricts to frames-per-second setting for frequency of updates */
int do_screenhandlers(bScreen *sc)
{
	static double ltime=0.0;
	double swaptime, time;
	short a, done= 0;
	
	time = PIL_check_seconds_timer();
	swaptime= 1.0/FPS;
	
	/* only now do the handlers */
	if(swaptime < time-ltime || ltime==0.0) {
		
		ltime= time;

		for(a=0; a<SCREEN_MAXHANDLER; a+=2) {
			switch(sc->handler[a]) {
				case SCREEN_HANDLER_ANIM:
					animated_screen(sc, sc->handler[a+1]);
					done= 1;
					break;
				case SCREEN_HANDLER_PYTHON:
					done= 1;
					break;
				case SCREEN_HANDLER_VERSE:
#ifdef WITH_VERSE
					b_verse_update();
#endif

					done= 1;
					break;
			}
		}
	}
	else if( qtest()==0) PIL_sleep_ms(5);   // 5 milliseconds pause, for idle

	/* separate check for if we need to add to afterqueue */
	/* is only to keep mainqueue awqke */
	for(a=0; a<SCREEN_MAXHANDLER; a+=2) {
		if(sc->handler[a]) {
			ScrArea *sa= sc->areabase.first;
			if(sa->headwin) addafterqueue(sa->headwin, SCREEN_HANDLER, 1);
			else addafterqueue(sa->win, SCREEN_HANDLER, 1);
		}
	}
	
	return done;
}

/* ****** end screen handlers ************ */

static void drawscreen(void)
{
	ScrArea *sa;
	
	mywinset(G.curscreen->mainwin);
	myortho2(-0.375, (float)G.curscreen->sizex-0.375, -0.375, (float)G.curscreen->sizey-0.375);
	
	sa= G.curscreen->areabase.first;
	while(sa) {
		drawscredge_area(sa);
		sa= sa->next;
	}
	
	/* this double draw patch seems to be needed for certain sgi's (octane, indigo2) */
#if defined(__sgi) || defined(__sun__) || defined( __sun ) || defined (__sparc) || defined (__sparc__)
	glDrawBuffer(GL_FRONT);
	
	sa= G.curscreen->areabase.first;
	while(sa) {
		drawscredge_area(sa);
		sa= sa->next;
	}
	
	glDrawBuffer(GL_BACK);
#endif
}

static void screen_dispatch_events(void) {
	int events_remaining= 1;
	ScrArea *sa;

	window_make_active(mainwin); // added it here instead of screenmain (ton)

	while (events_remaining) {
		events_remaining= 0;
				
		winqueue_break= 0;
		for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
				/* first check header, then rest. Header sometimes has initialization code */
			if (sa->headwin && bwin_qtest(sa->headwin)) {
				scrarea_dispatch_header_events(sa);
				events_remaining= 1;
			}
			if (winqueue_break) break;

			if (bwin_qtest(sa->win)) {
				scrarea_dispatch_events(sa);
				events_remaining= 1;
			}
			if (winqueue_break) break;
		}

		if (winqueue_break) break;
	}

	/* winqueue_break isnt the best of all solutions... but it is called on switching screens,
		so drawing should wait for all redraw/init events to be handled */
	if (winqueue_break==0) {
		if (dodrawscreen) {
			drawscreen();
			dodrawscreen= 0;
		}
		
		screen_swapbuffers();
		do_screenhandlers(G.curscreen);
	}
}

static ScrArea *screen_find_area_for_pt(bScreen *sc, short *mval) 
{
	ScrArea *sa;
	
	/* hotspot area of 1 pixel extra */
	
	for (sa= sc->areabase.first; sa; sa= sa->next) {
		if( sa->totrct.xmin + 1 < mval[0] )
			if( sa->totrct.ymin + 1 < mval[1] )
				if( sa->totrct.xmax - 1 > mval[0] )
					if( sa->totrct.ymax - 1 > mval[1] )
						return sa;
	}
	return NULL;
}

/* ugly yah, will disappear on better event system */
/* is called from interface.c after button events */
static char delayed_undo_name[64];
void screen_delayed_undo_push(char *name)
{
	strncpy(delayed_undo_name, name, 63);
	mainqenter(UNDOPUSH, 1);
}

void screenmain(void)
{
	int has_input= 1;
	int firsttime = 1;
	int onload_script = 0;

	window_make_active(mainwin);
	
	while (1) {
		unsigned short event;
		short val, towin;
		char ascii;

		flush_extqd_events();
		if (nafterqitems && !qtest()) {
			append_afterqueue();
			event= val= ascii= 0;
		} else {
			event= screen_qread(&val, &ascii);
		}
		
		// window_make_active(mainwin); // (only for inputchange, removed, (ton))

		if (event==INPUTCHANGE) {
			window_make_active(mainwin);
			G.qual= get_qual();
		}
		
			/* If the main window is active, find the current active ScrArea
			 * underneath the mouse cursor, updating the headers & cursor for
			 * the appropriate internal window if things have changed.
			 * 
			 * If the main window is not active, deactivate the internal 
			 * window.
			 */
		if (has_input || g_activearea==NULL || G.curscreen->winakt) {
			ScrArea *newactarea;
			int newactwin;
			short mval[2];

			getmouseco_sc(mval);
			newactarea= screen_find_area_for_pt(G.curscreen, mval);			

			if (newactarea) {
				if (BLI_in_rcti(&newactarea->headrct, mval[0], mval[1])) {
					newactwin= newactarea->headwin;
					set_cursor(CURSOR_STD);
				} else {
					newactwin= newactarea->win;
				}
			} else {
				newactwin= 0;
			}

			if (newactarea && (newactarea != g_activearea)) {
				if (g_activearea) scrarea_queue_headredraw(g_activearea);
				scrarea_queue_headredraw(newactarea);
				if (!(BLI_in_rcti(&newactarea->headrct, mval[0], mval[1]))) /* header always gets std cursor */
					set_cursor(newactarea->cursor);
				g_activearea= newactarea;
			}
			/* when you move mouse from header to window, buttons can remain hilited otherwise */
			if(newactwin != G.curscreen->winakt) {
				if (g_activearea) scrarea_queue_headredraw(g_activearea);
			}
			G.curscreen->winakt= newactwin;
			
			if (G.curscreen->winakt) {
				areawinset(G.curscreen->winakt);
				if (!(BLI_in_rcti(&newactarea->headrct, mval[0], mval[1]))) /* header always gets std cursor */
					set_cursor(choose_cursor(g_activearea));
			}
		} 
		else {
			if (g_activearea) {
				scrarea_queue_headredraw(g_activearea);
			}
			g_activearea= NULL;
			G.curscreen->winakt= 0;
		}

		towin= 0;
		if (event==WINCLOSE) {
			exit_usiblender();
		} 
		else if (event==DRAWEDGES) {
			dodrawscreen= 1;
		}
		else if (event==RESHAPE) {
			init_mainwin();
			markdirty_all();
			dodrawscreen= 1;
		}
		else if (event==REDRAW) {
			markdirty_all();
			dodrawscreen= 1;
		}
		else if( event==UNDOPUSH) {
			BIF_undo_push(delayed_undo_name);
		}
		else if (event==AUTOSAVE_FILE) {
			BIF_write_autosave();
		}
		else if (event==LOAD_FILE) {
			BIF_read_file(ext_load_str);
			sound_initialize_sounds();
		}
		else if ((event==ONLOAD_SCRIPT) && BPY_has_onload_script()) {
			/* event queued in setup_app_data() in blender.c, where G.f is checked */
			onload_script = 1;
			firsttime = 1; /* see last 'if' in this function */
		}
		else {
			towin= 1;
		}

		if (!g_activearea) {
			towin= 0;
		}
		else if (event==QKEY) {
			/* Temp place to print mem debugging info ctrl+alt+shift + qkey */
			if ( G.qual == (LR_SHIFTKEY | LR_ALTKEY | LR_CTRLKEY) ) {
				MEM_printmemlist_pydict();
			}
			
			else if((G.obedit && G.obedit->type==OB_FONT && g_activearea->spacetype==SPACE_VIEW3D)||g_activearea->spacetype==SPACE_TEXT||g_activearea->spacetype==SPACE_SCRIPT);
			else {
				if(val && (G.qual == LR_CTRLKEY)) {
					if(okee("Quit Blender")) exit_usiblender();
				}
				towin= 0;
			}
		}
		else if (event==RIGHTARROWKEY) {
			if(textediting==0 && val && (G.qual & LR_CTRLKEY)) {
				bScreen *sc= G.curscreen->id.next;

				/* if screen is last, set it to first */
				if(sc == NULL) 
					sc= G.main->screen.first;
				
				if(is_allowed_to_change_screen(sc)) setscreen(sc);
				g_activearea= NULL;
				towin= 0;
			}
		}
		else if (event==LEFTARROWKEY) {
			if(textediting==0 && val && (G.qual & LR_CTRLKEY)) {
				bScreen *sc= G.curscreen->id.prev;
				
				/* if screen is first, set it to last */
				if(sc == NULL) 
					sc= G.main->screen.last;
				
				if(is_allowed_to_change_screen(sc)) setscreen(sc);
				g_activearea= NULL;
				towin= 0;
			}
		}
		else if (!G.curscreen->winakt) {
			ScrEdge *actedge;
			short mval[2];

			getmouseco_sc(mval);
			actedge= screen_find_active_scredge(G.curscreen, mval);

			if (actedge) {
				if (scredge_is_horizontal(actedge)) {
					set_cursor(CURSOR_Y_MOVE);
				} else {
					set_cursor(CURSOR_X_MOVE);
				}
				// this does global hotkeys too
				screen_edge_edit_event(g_activearea, actedge, event, val);
			} else {
				set_cursor(CURSOR_STD);
			}
			
			towin= 0;
		}
		else if (event==ZKEY) {
			if(val && G.qual==(LR_ALTKEY|LR_SHIFTKEY|LR_CTRLKEY)) {
				extern void set_debug_swapbuffers_ovveride(bScreen *sc, int mode);

				int which= pupmenu("Swapbuffers%t|Simple|Debug|DebugSwap|Redraw|Default|KillSwap");
					
				switch (which) {
				case 1: set_debug_swapbuffers_ovveride(G.curscreen, 's'); break;
				case 2: set_debug_swapbuffers_ovveride(G.curscreen, 'd'); break;
				case 3: set_debug_swapbuffers_ovveride(G.curscreen, 'f'); break;
				case 4: set_debug_swapbuffers_ovveride(G.curscreen, 'r'); break;
				case 5: set_debug_swapbuffers_ovveride(G.curscreen, 0); break;
				case 6: 
					if (g_activearea) {
						g_activearea->head_swap= 0;
						g_activearea->win_swap= 0;
					}
					break;
				}
				towin= 0;
			}
		}
		else if (event==SPACEKEY) {
			if ((g_activearea->spacetype!=SPACE_TEXT) &&
				( !((g_activearea->spacetype==SPACE_VIEW3D) && ((G.obedit) && G.obedit->type==OB_FONT)) ) &&
				val && 
				(G.qual & LR_SHIFTKEY)) {
					area_fullscreen();
					g_activearea= NULL;
					towin= 0;
			}
			else {
				if((G.obedit && G.obedit->type==OB_FONT && g_activearea->spacetype==SPACE_VIEW3D)||g_activearea->spacetype==SPACE_TEXT||g_activearea->spacetype==SPACE_SCRIPT||g_activearea->spacetype==SPACE_SEQ);
				else if(G.qual==0) {
					if(val) toolbox_n();
					towin= 0;
				}
			}
		}
		else if(ELEM(event, UPARROWKEY, DOWNARROWKEY)) {
			if(val && (G.qual & LR_CTRLKEY)) {
				area_fullscreen();
				g_activearea= NULL;
				towin= 0;
			}
		}

		if (towin && event) {
			if (blenderqread(event, val))	// the global keys
				addqueue_ext(G.curscreen->winakt, event, val, ascii);
		}

			/* only process subwindow queue's once the
			 * main queue has been emptyied.
			 */
		event= qtest();
		if (event==0 || event==EXECUTE) {
			screen_dispatch_events();
		}
		
		if(G.f & G_DEBUG) {
			GLenum error = glGetError();
			if (error)
				printf("GL error: %s\n", gluErrorString(error));
		}
		/* Bizar hack. The event queue has mutated... */
		if ( (firsttime) && (event == 0) ) {

			if (onload_script) {
				/* OnLoad scriptlink */
				BPY_do_pyscript(&G.scene->id, SCRIPT_ONLOAD);
				onload_script = 0;
			}
			else if (G.fileflags & G_FILE_AUTOPLAY) {
				// SET AUTOPLAY in G.flags for
				// other fileloads

				G.flags |= G_FILE_AUTOPLAY;
				area_autoplayscreen();

				// Let The Games Begin
				// fake a 'p' keypress

				mainqenter(PKEY, 1);
			} else {
				extern char datatoc_splash_jpg[];
				extern int datatoc_splash_jpg_size;

				//if (! ((G.main->versionfile >= G.version)
				//       || G.save_over)) {
					splash((void *)datatoc_splash_jpg,
					       datatoc_splash_jpg_size, NULL);
				//}
			}
			firsttime = 0;
		}
	}
}

#if 0
//#ifdef _WIN32	// FULLSCREEN
void mainwindow_toggle_fullscreen(int fullscreen)
{
	if (fullscreen) U.uiflag |= USER_FLIPFULLSCREEN;
	else U.uiflag &= ~USER_FLIPFULLSCREEN;

	window_toggle_fullscreen(mainwin, fullscreen);
}
#endif

void mainwindow_raise(void) 
{
	if(mainwin)
		window_raise(mainwin);
}

void mainwindow_make_active(void) 
{
	if(mainwin)
		window_make_active(mainwin);
}

void mainwindow_close(void) 
{
	if(mainwin)
		window_destroy(mainwin);
	mainwin= NULL;
}

void mainwindow_set_filename_to_title(char *filename)
{
	char str[FILE_MAXDIR + FILE_MAXFILE];
	char dir[FILE_MAXDIR];
	char file[FILE_MAXFILE];

	BLI_split_dirfile_basic(filename, dir, file);

	if(BLI_streq(file, ".B.blend") || filename[0] =='\0')
		sprintf(str, "Blender");
	else
		sprintf(str, "Blender [%s]", filename);

	window_set_title(mainwin, str);
}

/* *********  AREAS  ************* */

void setprefsize(int stax, int stay, int sizx, int sizy, int maximized)
{
	int scrwidth, scrheight;
	
	winlay_get_screensize(&scrwidth, &scrheight);
		
	if(sizx<320) sizx= 320;
	if(sizy<256) sizy= 256;

	if(stay+sizy>scrheight) {
		fprintf(stderr," height prob \n");
		sizy= scrheight-stay;
	}

	if(sizx<320 || sizy<256) {
		printf("ERROR: illegal prefsize\n");
		return;
	}
	
	prefstax= stax;
	prefstay= stay;
	prefsizx= sizx;
	prefsizy= sizy;

	start_maximized= maximized;
}


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

static void removedouble_scrverts(void)
{
	ScrVert *v1, *verg;
	ScrEdge *se;
	ScrArea *sa;
	
	verg= G.curscreen->vertbase.first;
	while(verg) {
		if(verg->newv==0) {	/* !!! */
			v1= verg->next;
			while(v1) {
				if(v1->newv==0) {	/* !?! */
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
	se= G.curscreen->edgebase.first;
	while(se) {
		if(se->v1->newv) se->v1= se->v1->newv;
		if(se->v2->newv) se->v2= se->v2->newv;
		/* edges changed: so.... */
		sortscrvert(&(se->v1), &(se->v2));
		se= se->next;
	}
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa->v1->newv) sa->v1= sa->v1->newv;
		if(sa->v2->newv) sa->v2= sa->v2->newv;
		if(sa->v3->newv) sa->v3= sa->v3->newv;
		if(sa->v4->newv) sa->v4= sa->v4->newv;
		sa= sa->next;
	}
	
	/* remove */
	verg= G.curscreen->vertbase.first;
	while(verg) {
		v1= verg->next;
		if(verg->newv) {
			BLI_remlink(&G.curscreen->vertbase, verg);
			MEM_freeN(verg);
		}
		verg= v1;
	}
	
}

static void removenotused_scrverts(void)
{
	ScrVert *sv, *svn;
	ScrEdge *se;

	/* we assume edges are ok */
	
	se= G.curscreen->edgebase.first;
	while(se) {
		se->v1->flag= 1;
		se->v2->flag= 1;
		se= se->next;
	}
	
	sv= G.curscreen->vertbase.first;
	while(sv) {
		svn= sv->next;
		if(sv->flag==0) {
			BLI_remlink(&G.curscreen->vertbase, sv);
			MEM_freeN(sv);
		}
		else sv->flag= 0;
		sv= svn;
	}
}

static void removedouble_scredges(void)
{
	ScrEdge *verg, *se, *sn;
	
	/* compare */
	verg= G.curscreen->edgebase.first;
	while(verg) {
		se= verg->next;
		while(se) {
			sn= se->next;
			if(verg->v1==se->v1 && verg->v2==se->v2) {
				BLI_remlink(&G.curscreen->edgebase, se);
				MEM_freeN(se);
			}
			se= sn;
		}
		verg= verg->next;
	}
}

static void removenotused_scredges(void)
{
	ScrEdge *se, *sen;
	ScrArea *sa;
	int a=0;
	
	/* sets flags when edge is used in area */
	sa= G.curscreen->areabase.first;
	while(sa) {
		se= screen_findedge(G.curscreen, sa->v1, sa->v2);
		if(se==0) printf("error: area %d edge 1 bestaat niet\n", a);
		else se->flag= 1;
		se= screen_findedge(G.curscreen, sa->v2, sa->v3);
		if(se==0) printf("error: area %d edge 2 bestaat niet\n", a);
		else se->flag= 1;
		se= screen_findedge(G.curscreen, sa->v3, sa->v4);
		if(se==0) printf("error: area %d edge 3 bestaat niet\n", a);
		else se->flag= 1;
		se= screen_findedge(G.curscreen, sa->v4, sa->v1);
		if(se==0) printf("error: area %d edge 4 bestaat niet\n", a);
		else se->flag= 1;
		sa= sa->next;
		a++;
	}
	se= G.curscreen->edgebase.first;
	while(se) {
		sen= se->next;
		if(se->flag==0) {
			BLI_remlink(&G.curscreen->edgebase, se);
			MEM_freeN(se);
		}
		else se->flag= 0;
		se= sen;
	}
}

void calc_arearcts(ScrArea *sa)
{

	if(sa->v1->vec.x>0) sa->totrct.xmin= sa->v1->vec.x+1;
	else sa->totrct.xmin= sa->v1->vec.x;
	if(sa->v4->vec.x<G.curscreen->sizex-1) sa->totrct.xmax= sa->v4->vec.x-1;
	else sa->totrct.xmax= sa->v4->vec.x;
	
	if(sa->v1->vec.y>0) sa->totrct.ymin= sa->v1->vec.y+1;
	else sa->totrct.ymin= sa->v1->vec.y;
	if(sa->v2->vec.y<G.curscreen->sizey-1) sa->totrct.ymax= sa->v2->vec.y-1;
	else sa->totrct.ymax= sa->v2->vec.y;
	
	sa->winrct= sa->totrct;
	sa->headrct= sa->totrct;
	if(sa->headertype) {
		if(sa->headertype==HEADERDOWN) {
			sa->headrct.ymax= sa->headrct.ymin+HEADERY;
			sa->winrct.ymin= sa->headrct.ymax+1;
		}
		else if(sa->headertype==HEADERTOP) {
			sa->headrct.ymin= sa->headrct.ymax-HEADERY;
			sa->winrct.ymax= sa->headrct.ymin-1;
		}
	}
	else {
		sa->headrct.ymax= sa->headrct.ymin;
	}
	if(sa->winrct.ymin>sa->winrct.ymax) sa->winrct.ymin= sa->winrct.ymax;
	
	/* for speedup */
	sa->winx= sa->winrct.xmax-sa->winrct.xmin+1;
	sa->winy= sa->winrct.ymax-sa->winrct.ymin+1;
}

static void openheadwin(ScrArea *sa)
{
	sa->headwin= myswinopen(G.curscreen->mainwin,
		sa->headrct.xmin, sa->headrct.xmax, sa->headrct.ymin, sa->headrct.ymax);

	glMatrixMode(GL_MODELVIEW);
	
	areawinar[sa->headwin]= sa;	/* otherwise addqueue does not work */
	
	scrarea_do_headchange(sa);	/* headchange is no callback, apply right away. this is for render-to-imagewindow... this can be called on startup by sequencer, which invokes redraw before all events are handled. bad stuff... */
	addqueue(sa->headwin, CHANGED, 1);
}

static void openareawin(ScrArea *sa)
{
	sa->win= myswinopen(G.curscreen->mainwin, 
		sa->winrct.xmin, sa->winrct.xmax, sa->winrct.ymin, sa->winrct.ymax);

	areawinar[sa->win]= sa;	/* otherwise addqueue does not work */
	addqueue(sa->win, CHANGED, 1);
}

static void closeheadwin(ScrArea *sa)
{
	if(sa->headwin) mywinclose(sa->headwin);
	sa->headwin= 0;
}

static void closeareawin(ScrArea *sa)
{
	uiFreeBlocksWin(&sa->uiblocks, sa->win);
	
	if(sa->win) mywinclose(sa->win);
	sa->win= 0;
}

static void del_area(ScrArea *sa)
{
	closeareawin(sa);
	closeheadwin(sa);

	freespacelist(sa);
	
	uiFreeBlocks(&sa->uiblocks);
	uiFreePanels(&sa->panels);
	
	BPY_free_scriptlink(&sa->scriptlink);
	
	if(sa==curarea) curarea= NULL;
	if(sa==g_activearea) g_activearea= NULL;
}

/* sa2 to sa1, we swap spaces for fullscreen to keep all allocated data */
static void copy_areadata(ScrArea *sa1, ScrArea *sa2, int swap_space)
{
	Panel *pa1, *pa2, *patab;
	ScriptLink *slink1 = &sa1->scriptlink, *slink2 = &sa2->scriptlink;

	sa1->headertype= sa2->headertype;
	sa1->spacetype= sa2->spacetype;
	Mat4CpyMat4(sa1->winmat, sa2->winmat);

	if(swap_space) {
		SWAP(ListBase, sa1->spacedata, sa2->spacedata);
		/* exception: ensure preview is reset */
		if(sa1->spacetype==SPACE_VIEW3D)
			BIF_view3d_previewrender_free(sa1->spacedata.first);
	}
	else {
		freespacelist(sa1);
		duplicatespacelist(sa1, &sa1->spacedata, &sa2->spacedata);
	}
	
	BLI_freelistN(&sa1->panels);
	duplicatelist(&sa1->panels, &sa2->panels);

	/* space handler script links */
	if (slink1->totscript) {
		MEM_freeN(slink1->scripts);
		MEM_freeN(slink1->flag);
		slink1->totscript = 0;
	}
	if (slink2->totscript) {
		slink1->scripts = MEM_dupallocN(slink2->scripts);
		slink1->flag = MEM_dupallocN(slink2->flag);
		slink1->totscript = slink2->totscript;
	}

	/* copy pointers */
	pa1= sa1->panels.first;
	while(pa1) {
		
		patab= sa1->panels.first;
		pa2= sa2->panels.first;
		while(patab) {
			if( pa1->paneltab == pa2) {
				pa1->paneltab = patab;
				break;
			}
			patab= patab->next;
			pa2= pa2->next;
		}
		pa1= pa1->next;
	}
}

static ScrArea *screen_addarea(bScreen *sc, ScrVert *v1, ScrVert *v2, ScrVert *v3, ScrVert *v4, short headertype, short spacetype)
{
	ScrArea *sa= MEM_callocN(sizeof(ScrArea), "addscrarea");
	sa->cursor= CURSOR_STD;
	sa->v1= v1;
	sa->v2= v2;
	sa->v3= v3;
	sa->v4= v4;
	sa->headertype= headertype;
	sa->spacetype= spacetype;

	calc_arearcts(sa);

	if (sa->headertype) openheadwin(sa);
	openareawin(sa);

	BLI_addtail(&sc->areabase, sa);
	return sa;
}

static int rcti_eq(rcti *a, rcti *b) {
	return ((a->xmin==b->xmin && a->xmax==b->xmax) &&
			(a->ymin==b->ymin && a->ymax==b->ymax));
}

static void testareas(void)
{
	ScrArea *sa, *next;

	/* test for header, if removed, or moved */
	/* test for window, if removed, or moved */
	
	for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		rcti oldhr= sa->headrct;
		rcti oldwr= sa->winrct;
		
		next= sa->next;
		
		calc_arearcts(sa);
		
		/* ilegally scaled down area.... */
		if(sa->totrct.xmin>=sa->totrct.xmax || sa->totrct.ymin>=sa->totrct.ymax) {
			del_area(sa);
			BLI_remlink(&G.curscreen->areabase, sa);
			MEM_freeN(sa);
			printf("Warning, removed zero sized window from screen %s\n", G.curscreen->id.name+2);
		}
		else {
				/* test header */
			if (sa->headwin) {
				if (!rcti_eq(&oldhr, &sa->headrct)) {
					mywinposition(sa->headwin, sa->headrct.xmin, sa->headrct.xmax, sa->headrct.ymin, sa->headrct.ymax);
					addqueue(sa->headwin, CHANGED, 1);
				}
					
				if(sa->headbutlen<sa->winx) {
					sa->headbutofs= 0;
					addqueue(sa->headwin, CHANGED, 1);
				}
				else if(sa->headbutofs+sa->winx > sa->headbutlen) {
					sa->headbutofs= sa->headbutlen-sa->winx;
					addqueue(sa->headwin, CHANGED, 1);
				}
			}

			if (!rcti_eq(&oldwr, &sa->winrct)) {
				SpaceLink *sl= sa->spacedata.first;
				
				mywinposition(sa->win, sa->winrct.xmin, sa->winrct.xmax, sa->winrct.ymin, sa->winrct.ymax);
				addqueue(sa->win, CHANGED, 1);
				
				/* exception handling... probably we need generic event */
				for(; sl; sl= sl->next)
					if(sl->spacetype==SPACE_VIEW3D)
						BIF_view3d_previewrender_free((View3D *)sl);
			}
		}
	}
	
		/* remake global windowarray */
	memset(areawinar, 0, sizeof(void *)*MAXWIN);
	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		areawinar[sa->headwin]= sa;
		areawinar[sa->win]= sa;
	}
	
		/* test if winakt is OK */	
	if( areawinar[G.curscreen->winakt]==0) G.curscreen->winakt= 0;
}

static ScrArea *test_edge_area(ScrArea *sa, ScrEdge *se)
{
	/* test if edge is in area, if not, 
	   then find an area that has it */
  
	ScrEdge *se1=0, *se2=0, *se3=0, *se4=0;
	
	if(sa) {
		se1= screen_findedge(G.curscreen, sa->v1, sa->v2);
		se2= screen_findedge(G.curscreen, sa->v2, sa->v3);
		se3= screen_findedge(G.curscreen, sa->v3, sa->v4);
		se4= screen_findedge(G.curscreen, sa->v4, sa->v1);
	}
	if(se1!=se && se2!=se && se3!=se && se4!=se) {
		
		sa= G.curscreen->areabase.first;
		while(sa) {
			/* a bit optimise? */
			if(se->v1==sa->v1 || se->v1==sa->v2 || se->v1==sa->v3 || se->v1==sa->v4) {
				se1= screen_findedge(G.curscreen, sa->v1, sa->v2);
				se2= screen_findedge(G.curscreen, sa->v2, sa->v3);
				se3= screen_findedge(G.curscreen, sa->v3, sa->v4);
				se4= screen_findedge(G.curscreen, sa->v4, sa->v1);
				if(se1==se || se2==se || se3==se || se4==se) return sa;
			}
			sa= sa->next;
		}
	}

	return sa;	/* is null when not find */
}

ScrArea *closest_bigger_area(void)
{
	ScrArea *sa, *big=0;
	float cent[3], vec[3],len, len1, len2, len3, dist=1000;
	short mval[2];
	
	getmouseco_sc(mval);
	
	cent[0]= mval[0];
	cent[1]= mval[1];
	cent[2]= vec[2]= 0;

	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa!=curarea) {
			if(sa->winy>=curarea->winy) {
			
				/* mimimum of the 4 corners */
				vec[0]= sa->v1->vec.x; vec[1]= sa->v1->vec.y;
				len= VecLenf(vec, cent);
				vec[0]= sa->v2->vec.x; vec[1]= sa->v2->vec.y;
				len1= VecLenf(vec, cent);
				vec[0]= sa->v3->vec.x; vec[1]= sa->v3->vec.y;
				len2= VecLenf(vec, cent);
				vec[0]= sa->v4->vec.x; vec[1]= sa->v4->vec.y;
				len3= VecLenf(vec, cent);
				
				len= MIN4(len, len1, len2, len3);
				
				/* plus center */
				vec[0]= (sa->v2->vec.x+sa->v3->vec.x)/2;
				vec[1]= (sa->v1->vec.y+sa->v2->vec.y)/2;

				len+= 0.5*VecLenf(vec, cent);
				
				/* min size */
				len-= sa->winy+sa->winx;
				
				if(len<dist) {
					dist= len;
					big= sa;
				}
			}
		}
		sa= sa->next;
	}
	
	if(big) return big;
	else return curarea;
}

/* ************ SCREEN MANAGEMENT ************** */

static int statechanged= 0;
void BIF_wait_for_statechange(void)
{
	if (!statechanged) {
			/* Safety, don't wait more than 0.1 seconds */
		double stime= PIL_check_seconds_timer();
		while (!statechanged) {
			winlay_process_events(1);
			if ((PIL_check_seconds_timer()-stime)>0.1) break;
		}
		statechanged= 0;
	}
	else PIL_sleep_ms(3);	/* statechanged can be set '1' while holding mousebutton, causing locks */

}
void getmouse(short *mval)
{
	winlay_process_events(0);
	window_get_mouse(mainwin, mval);
}
short get_qual(void)
{
	winlay_process_events(0);
	return window_get_qual(mainwin);
}
short get_mbut(void)
{
	winlay_process_events(0);
	return window_get_mbut(mainwin);
}

/* return values of tablet data related functions are documented 
 * in the Window struct, ghostwinlay.c */
float get_pressure(void)
{
	winlay_process_events(0);
	return window_get_pressure(mainwin);
}
void get_tilt(float *xtilt, float *ytilt)
{
	winlay_process_events(0);
	window_get_tilt(mainwin, xtilt, ytilt);
}
short get_activedevice(void)
{
	winlay_process_events(0);
	return window_get_activedevice(mainwin);
}

void getndof(float *sbval)
{
    winlay_process_events(0);
    window_get_ndof(mainwin, sbval);
}

void filterNDOFvalues(float *sbval)
{
	int i=0;
	float max  = 0.0;
	
	for (i =0; i<6;i++)
		if (fabs(sbval[i]) > max)
			max = fabs(sbval[i]);
	for (i =0; i<6;i++)
		if (fabs(sbval[i]) != max )
			sbval[i]=0.0;
}

void add_to_mainqueue(Window *win, void *user_data, short evt, short val, char ascii)
{

	statechanged= 1;

	/*  accept the extended ascii set (ton) */
	if( !val || ascii<32 ) {
		ascii= '\0';
	}

	mainqenter_ext(evt, val, ascii);
}

/* ScrVert ordering in a ScrArea:

2---------3
|         |
|         |
1---------4
  
*/

static bScreen *addscreen(char *name)		/* use setprefsize() if you want something else than a full windpw */
{
	/* this function sets variabele G.curscreen,
	 * that global is about used everywhere!
	 */
	bScreen *sc;
	ScrVert *sv1, *sv2, *sv3, *sv4;
	short startx, starty, endx, endy;	
	
	sc= G.curscreen= alloc_libblock(&G.main->screen, ID_SCR, name);

	if (!prefsizx) {
		prefstax= 0;
		prefstay= 0;
		
		winlay_get_screensize(&prefsizx, &prefsizy);
	}

	startx= prefstax;
	starty= prefstay;
	endx= prefstax+prefsizx-1;
	endy= prefstay+prefsizy-1;

	sc->startx= startx;	sc->starty= starty;
	sc->endx= endx;	sc->endy= endy;
	sc->sizex= sc->endx-sc->startx+1;
	sc->sizey= sc->endy-sc->starty+1;
	
	sc->scene= G.scene;
	
  	if (!mainwin) {
		if (G.windowstate == G_WINDOWSTATE_FULLSCREEN)
			mainwin= window_open("Blender", sc->startx, sc->starty, sc->sizex, sc->sizey, G_WINDOWSTATE_FULLSCREEN);
		else
			mainwin= window_open("Blender", sc->startx, sc->starty, sc->sizex, sc->sizey, start_maximized);
		
		if (!mainwin) {
			printf("ERROR: Unable to open Blender window\n");
			exit(1);
		}
		
		window_set_handler(mainwin, add_to_mainqueue, NULL);
		window_open_ndof(mainwin); /* needs to occur once the mainwin handler is set */
		init_mainwin();
		mywinset(1);
	
		/* for visual speed, but still needed? */
		glClearColor(.55, .55, .55, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		window_swap_buffers(mainwin);
		
		/* this is unneeded and with large monitors can be a
		 * pain so commenting out */
		/* warp_pointer(sc->sizex/2,  sc->sizey/2); */
		
		mainqenter(REDRAW, 1);
	}

	sc->mainwin= 1;
	
	sv1= screen_addvert(sc, 0, 0);
	sv2= screen_addvert(sc, 0, sc->endy-sc->starty);
	sv3= screen_addvert(sc, sc->sizex-1, sc->sizey-1);
	sv4= screen_addvert(sc, sc->sizex-1, 0);
	
	screen_addedge(sc, sv1, sv2);
	screen_addedge(sc, sv2, sv3);
	screen_addedge(sc, sv3, sv4);
	screen_addedge(sc, sv4, sv1);

	screen_addarea(sc, sv1, sv2, sv3, sv4, HEADERDOWN, SPACE_INFO);
	
	G.curscreen= sc;

	return sc;
}

void setscreen(bScreen *sc)
{
	bScreen *sc1;
	ScrArea *sa;
	short mval[2];
	
	if(sc->full) {				/* find associated full */
		sc1= G.main->screen.first;
		while(sc1) {
			sa= sc1->areabase.first;
			if(sa->full==sc) {
				sc= sc1;
				break;
			}
			sc1= sc1->id.next;
		}
		if(sc1==0) printf("setscreen error\n");
	}

	/* de-activate G.curscreen */
	if (G.curscreen && G.curscreen != sc) {
		sa= G.curscreen->areabase.first;
		while(sa) {
			if(sa->win) mywinclose(sa->win);
			sa->win= 0;
			if(sa->headwin) mywinclose(sa->headwin);
			sa->headwin= 0;
			
			uiFreeBlocks(&sa->uiblocks);
			
			sa= sa->next;
		}		
	}
	else if(G.curscreen) markdirty_all();	/* at least redraw */

	if (G.curscreen != sc) {
		mywinset(sc->mainwin);
	}
	
	G.curscreen= sc;

	for (sa= sc->areabase.first; sa; sa= sa->next) {
			/* XXX, fixme zr */
/* 		if (sa->win || sa->headwin) */
/* 			printf("error in setscreen (win): %d, %d\n", sa->win, sa->headwin); */
		if (!sa->win)
			openareawin(sa);
		if (!sa->headwin && sa->headertype)
			openheadwin(sa);
	}

	/* recalculate winakt */
	getmouseco_sc(mval);

	test_scale_screen(sc);
	testareas();
	
	for(sa= sc->areabase.first; sa; sa= sa->next) {
		SpaceLink *sl;
		
		for(sl= sa->spacedata.first; sl; sl= sl->next) {
			sl->area= sa;

			if(sl->spacetype==SPACE_OOPS) {
				SpaceOops *soops= (SpaceOops *) sl;

				/* patch for old files */
				if(soops->v2d.cur.xmin==soops->v2d.cur.xmax) {
					init_v2d_oops(sa, soops);
				}
			}
			else if(sl->spacetype==SPACE_BUTS) {
				SpaceButs *sbuts= (SpaceButs *)sl;
				sbuts->re_align= 1;		// force an align call, maybe new panels were added, also for after file reading
			}
		}
		
		sa->cursor= CURSOR_STD;
	}
	
	if(G.scene!=sc->scene)
		set_scene(sc->scene);

	countall();
	
	G.curscreen->winakt= 0;
	curarea= sc->areabase.first;
	
	mainqenter(DRAWEDGES, 1);
	dodrawscreen= 1;		/* patch! even gets lost,,,? */

	winqueue_break= 1;		/* means leave queue everywhere */
}

static void splitarea(ScrArea *sa, char dir, float fac);

void area_fullscreen(void)	/* with curarea */
{
	/* this function toggles: if area is full then the parent will be restored */
	bScreen *sc, *oldscreen;
	ScrArea *sa, *newa, *old;
	short headertype, fulltype;
	
	if(curarea->full) {
		sc= curarea->full;	/* the old screen */
		fulltype = sc->full;

		// refuse to go out of SCREENAUTOPLAY as long as G_FLAGS_AUTOPLAY
		// is set

		if (fulltype != SCREENAUTOPLAY || (G.flags & G_FILE_AUTOPLAY) == 0) {
			sc->full= 0;
		
			/* find old area */
			old= sc->areabase.first;
			while(old) {
				if(old->full) break;
				old= old->next;
			}
			if(old==0) {error("something wrong in areafullscreen"); return;}
		
			if (fulltype == SCREENAUTOPLAY) {
				// in autoplay screens the headers are disabled by 
				// default. So use the old headertype instead
				headertype = old->headertype;
			} else {
				// normal fullscreen. Use current headertype
				headertype = curarea->headertype;
			}

			copy_areadata(old, curarea, 1);	/*  1 = swap spacelist */
			old->headertype = headertype;

			old->full= 0;
		
			unlink_screen(G.curscreen);
			free_libblock(&G.main->screen, G.curscreen);
			G.curscreen= NULL;

			setscreen(sc);
		}
		
	}
	else {
		/* is there only 1 area? */
		if(G.curscreen->areabase.first==G.curscreen->areabase.last) return;
		if(curarea->spacetype==SPACE_INFO) return;
		
		G.curscreen->full = SCREENFULL;
		
		old= curarea;		
		oldscreen= G.curscreen;
		sc= addscreen("temp");		/* this sets G.curscreen */

		splitarea( (ScrArea *)sc->areabase.first, 'h', 0.99);
		newa= sc->areabase.first;
		newspace(newa->next, SPACE_INFO);
		
		curarea= old;
		G.curscreen= oldscreen;	/* needed because of setscreen */
		
		/* copy area */
		copy_areadata(newa, curarea, 1);	/* 1 = swap spacelist */
		
		curarea->full= oldscreen;
		newa->full= oldscreen;
		newa->next->full= oldscreen;
		
		setscreen(sc);
		wich_cursor(newa);
	}
	
	/* there's also events in queue for this, but we call fullscreen for render output
	now, and that doesn't go back to queue. Bad code, but doesn't hurt... (ton) */
	for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		scrarea_do_headchange(sa);
		scrarea_do_winchange(sa);
	}
	/* bad code #2: setscreen() ends with first area active. fullscreen render assumes this too */
	curarea= sc->areabase.first;
	
	retopo_force_update();
}

static void area_autoplayscreen(void)
{
	bScreen *sc, *oldscreen;
	ScrArea *newa, *old, *sa;
	
	if (curarea->full) {
		area_fullscreen();
	}

	if (curarea->full == NULL) { 
		sa = G.curscreen->areabase.first;
		while (sa) {
			if (sa->spacetype == SPACE_VIEW3D) {
				break;
			}
			sa= sa->next;
		}

		if (sa) {
			areawinset(sa->win);
			G.curscreen->full = SCREENAUTOPLAY;
			
			old= curarea;		
			oldscreen= G.curscreen;
			sc= addscreen("temp");		/* this sets G.curscreen */
	
			newa= sc->areabase.first;
			
			curarea= old;
			G.curscreen= oldscreen;	/* because of setscreen */
			
			/* copy area settings */
			copy_areadata(newa, curarea, 1);	/* swap spacedata */
			newa->headertype= 0;
			
			curarea->full= oldscreen;
			newa->full= oldscreen;
	
			setscreen(sc);
			wich_cursor(newa);
		}
	}
}

static void copy_screen(bScreen *to, bScreen *from)
{
	ScrVert *s1, *s2;
	ScrEdge *se;
	ScrArea *sa, *saf;

	/* free 'to' */
	free_screen(to);
	winqueue_break= 1;	/* leave queues everywhere */
	
	duplicatelist(&to->vertbase, &from->vertbase);
	duplicatelist(&to->edgebase, &from->edgebase);
	duplicatelist(&to->areabase, &from->areabase);
	
	s1= from->vertbase.first;
	s2= to->vertbase.first;
	while(s1) {
		s1->newv= s2;
		s2= s2->next;
		s1= s1->next;
	}
	se= to->edgebase.first;
	while(se) {
		se->v1= se->v1->newv;
		se->v2= se->v2->newv;
		sortscrvert(&(se->v1), &(se->v2));
		se= se->next;
	}

	sa= to->areabase.first;
	saf= from->areabase.first;
	while(sa) {
		sa->v1= sa->v1->newv;
		sa->v2= sa->v2->newv;
		sa->v3= sa->v3->newv;
		sa->v4= sa->v4->newv;
		sa->win= 0;
		sa->headwin= 0;
		
		sa->spacedata.first= sa->spacedata.last= NULL;
		sa->uiblocks.first= sa->uiblocks.last= NULL;
		sa->panels.first= sa->panels.last= NULL;
		sa->scriptlink.totscript= 0;
		
		copy_areadata(sa, saf, 0);
		
		sa= sa->next;
		saf= saf->next;
	}
	
	/* put at zero (needed?) */
	s1= from->vertbase.first;
	while(s1) {
		s1->newv= 0;
		s1= s1->next;
	}
}

void duplicate_screen(void)
{
	bScreen *sc, *oldscreen;
	
	if(G.curscreen->full != SCREENNORMAL) return;
	
	/* make new screen: */

	oldscreen= G.curscreen;
	sc= addscreen(oldscreen->id.name+2);	/* this sets G.curscreen */
	copy_screen(sc, oldscreen);

	G.curscreen= oldscreen;
	setscreen(sc);

}


/* ************ END SCREEN MANAGEMENT ************** */
/* ************  JOIN/SPLIT/MOVE ************** */

typedef struct point{
	float x,y;
}_point;

/* draw vertical shape visualising future joining (left as well
 * right direction of future joining) */
static void draw_horizontal_join_shape(ScrArea *sa, char dir)
{
	_point points[10];
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
	_point points[10];
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
	glColor4ub(0, 0, 0, 105);
	draw_join_shape(sa, dir);
	glDisable(GL_BLEND);
}

/* draw screen area ligher with arrow shape ("eraser" of previous dark shape) */
static void scrarea_draw_shape_light(ScrArea *sa, char dir)
{
	glBlendFunc(GL_DST_COLOR, GL_SRC_ALPHA);
	glEnable(GL_BLEND);
	/* value 181 was hardly computed: 181~105 */
	glColor4ub(255, 255, 255, 181);		
	draw_join_shape(sa, dir);
	glDisable(GL_BLEND);
}

static void joinarea_interactive(ScrArea *area, ScrEdge *onedge)
{
	struct ScrArea *sa1 = area, *sa2, *scr;
	struct ScrArea *up=0, *down=0, *right=0, *left=0;
	struct ScrEdge *se;
	unsigned short event;
	short ok=0, val=0, mval[2];
	char dir=0;

	sa1 = test_edge_area(sa1, onedge);
	if(sa1==0) return;

	/* find directions with same edge */
	sa2= G.curscreen->areabase.first;
	while(sa2) {
		if(sa2 != sa1) {
			se= screen_findedge(G.curscreen, sa2->v1, sa2->v2);
			if(onedge==se) right= sa2;
			se= screen_findedge(G.curscreen, sa2->v2, sa2->v3);
			if(onedge==se) down= sa2;
			se= screen_findedge(G.curscreen, sa2->v3, sa2->v4);
			if(onedge==se) left= sa2;
			se= screen_findedge(G.curscreen, sa2->v4, sa2->v1);
			if(onedge==se) up= sa2;
		}
		sa2= sa2->next;
	}
	
	if(left) val++;
	if(up) val++;
	if(right) val++;
	if(down) val++;
	
	if(val==0) return;
	else if(val==1) {
		if(left) {
			right = sa1;
			sa2 = left;
			dir = 'h';
		}
		else if(right) {
			left = sa1;
			sa2 = right;
			dir = 'h';
		}
		else if(up) {
			down = sa1;
			sa2= up;
			dir = 'v';
		}
		else if(down) {
			up = sa1;
			sa2 = down;
			dir = 'v';
		}
	}

	mywinset(G.curscreen->mainwin);

	/* initial set up screen area asigned for destroying */
	scr = sa2;

	/* set up standard cursor */
	set_cursor(CURSOR_STD);

	/* should already have a good matrix */
	glReadBuffer(GL_FRONT);
	glDrawBuffer(GL_FRONT);

	/* to prevent flickering after clicking at "Join Areas " */
	getmouseco_sc(mval);
	if(dir=='h') {
		if(scr==left && mval[0]>=onedge->v1->vec.x) scr = right;
		else if(scr==right && mval[0]<onedge->v1->vec.x) scr = left;
	}
	else if(dir=='v') {
		if(scr==down && mval[1]>=onedge->v1->vec.y) scr = up;
		else if(scr==up && mval[1]<onedge->v1->vec.y) scr = down;
	}

	/* draw scr screen area with dark shape */
	if(scr==left)
		scrarea_draw_shape_dark(scr,'r');
	else if(scr==right)
		scrarea_draw_shape_dark(scr,'l');
	else if(scr==up)
		scrarea_draw_shape_dark(scr,'d');
	else if(scr==down)
		scrarea_draw_shape_dark(scr,'u');
	bglFlush();

	/* "never ending loop" of interactive selection */
	while(!ok) {
		getmouseco_sc(mval);

		/* test if position of mouse is on the "different side" of
		 * "joining edge" */
		if(dir=='h') {
			if(scr==left && mval[0]>=onedge->v1->vec.x) {
				scrarea_draw_shape_light(scr,'r');
				scr = right;
				scrarea_draw_shape_dark(scr,'l');
			}
			else if(scr==right && mval[0]<onedge->v1->vec.x) {
				scrarea_draw_shape_light(scr,'l');
				scr = left;
				scrarea_draw_shape_dark(scr,'r');
			}
		}
		else if(dir=='v') {
			if(scr==down && mval[1]>=onedge->v1->vec.y) {
				scrarea_draw_shape_light(scr,'u');
				scr = up;
				scrarea_draw_shape_dark(scr,'d');
			}
			else if(scr==up && mval[1]<onedge->v1->vec.y){
				scrarea_draw_shape_light(scr,'d');
				scr = down;
				scrarea_draw_shape_dark(scr,'u');
			}
		}


		/* get pressed keys and mouse buttons */
		event = extern_qread(&val);

		/* confirm joining of two screen areas */
		if(val && event==LEFTMOUSE) ok= 1;

		/* cancel joining of joining */
		if(val && (event==ESCKEY || event==RIGHTMOUSE)) ok= -1;

		bglFlush();
	}

	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_BACK);

	/* joining af screen areas was confirmed ... proceed joining */
	if(ok==1) {
		if(sa2!=scr) {
			sa1 = sa2;
			sa2 = scr;
		}

		if(sa2==left) {
			sa1->v1= sa2->v1;
			sa1->v2= sa2->v2;
			screen_addedge(G.curscreen, sa1->v2, sa1->v3);
			screen_addedge(G.curscreen, sa1->v1, sa1->v4);
		}
		else if(sa2==up) {
			sa1->v2= sa2->v2;
			sa1->v3= sa2->v3;
			screen_addedge(G.curscreen, sa1->v1, sa1->v2);
			screen_addedge(G.curscreen, sa1->v3, sa1->v4);
		}
		else if(sa2==right) {
			sa1->v3= sa2->v3;
			sa1->v4= sa2->v4;
			screen_addedge(G.curscreen, sa1->v2, sa1->v3);
			screen_addedge(G.curscreen, sa1->v1, sa1->v4);
		}
		else if(sa2==down) {
			sa1->v1= sa2->v1;
			sa1->v4= sa2->v4;
			screen_addedge(G.curscreen, sa1->v1, sa1->v2);
			screen_addedge(G.curscreen, sa1->v3, sa1->v4);
		}
	
		del_area(sa2);
		BLI_remlink(&G.curscreen->areabase, sa2);
		MEM_freeN(sa2);
		
		removedouble_scredges();
		removenotused_scredges();
		removenotused_scrverts();
		
		testareas();
		mainqenter(DRAWEDGES, 1);

		/* test cursor en inputwindow */
		mainqenter(MOUSEY, -1);
	}
}

static short testsplitpoint(ScrArea *sa, char dir, float fac)
/* return 0: no split possible */
/* else return (integer) screencoordinate split point */
{
	short x, y;
	
	/* area big enough? */
	if(sa->v4->vec.x- sa->v1->vec.x <= 2*AREAMINX) return 0;
	if(sa->v2->vec.y- sa->v1->vec.y <= 2*AREAMINY) return 0;

	/* to be sure */
	if(fac<0.0) fac= 0.0;
	if(fac>1.0) fac= 1.0;
	
	if(dir=='h') {
		y= sa->v1->vec.y+ fac*(sa->v2->vec.y- sa->v1->vec.y);
		
		if(sa->v2->vec.y==G.curscreen->sizey-1 && sa->v2->vec.y- y < HEADERY) 
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

static void splitarea(ScrArea *sa, char dir, float fac)
{
	bScreen *sc;
	ScrArea *newa=NULL;
	ScrVert *sv1, *sv2;
	short split;
	
	if(sa==0) return;
	
	split= testsplitpoint(sa, dir, fac);
	if(split==0) return;
	
	sc= G.curscreen;
	
	areawinset(sa->win);
	
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
		copy_areadata(newa, sa, 0);

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
		copy_areadata(newa, sa, 0);

		/* area right */
		sa->v1= sv1;		
		sa->v2= sv2;
	}
	
	if(sa->spacetype==SPACE_BUTS) {
		addqueue(sa->win, UI_BUT_EVENT, B_BUTSHOME);
		addqueue(newa->win, UI_BUT_EVENT, B_BUTSHOME);
	}
	
	/* remove double vertices en edges */
	removedouble_scrverts();
	removedouble_scredges();
	removenotused_scredges();
	
	mainqenter(DRAWEDGES, 1);
	dodrawscreen= 1;		/* patch! event gets lost,,,? */
	testareas();
}

static void scrarea_draw_splitpoint(ScrArea *sa, char dir, float fac)
{
	int split= testsplitpoint(sa, dir, fac);

	if (split) {
		if(dir=='h') {
			sdrawXORline(sa->totrct.xmin, split, sa->totrct.xmax, split);
			sdrawXORline(sa->totrct.xmin, split-1, sa->totrct.xmax, split-1);
		} else {
			sdrawXORline(split, sa->totrct.ymin, split, sa->totrct.ymax);
			sdrawXORline(split-1, sa->totrct.ymin, split-1, sa->totrct.ymax);
		}
	}
}

static void splitarea_interactive(ScrArea *area, ScrEdge *onedge)
{
	ScrArea *scr, *sa= area;
	float fac= 0.0;
	unsigned short event;
	short ok= 0, val, split = 0, mval[2], mvalo[2]= {-1, -1}, first= 1;
	char dir;
	
	if(sa->win==0) return;
	if(sa->full) return;
	if(myswinopen_allowed()==0) {
		error("Max amount of subwindows reached");
		return;
	}
	
	dir= scredge_is_horizontal(onedge)?'v':'h';
	
	mywinset(G.curscreen->mainwin);
	/* should already have a good matrix */
	glReadBuffer(GL_FRONT);
	glDrawBuffer(GL_FRONT);

	/* keep track of grid and minsize */
	while(ok==0) {
		getmouseco_sc(mval);

		/* this part of code allows to choose, what window will be splited */
		/* cursor is out of the current ScreenArea */
		if((mval[0] < sa->v1->vec.x) || (mval[0] > sa->v3->vec.x) ||
		(mval[1] < sa->v1->vec.y) || (mval[1] > sa->v3->vec.y)){
			scr= (ScrArea*)G.curscreen->areabase.first;
			while(scr){
				if((mval[0] > scr->v1->vec.x) && (mval[0] < scr->v4->vec.x) &&
				(mval[1] < scr->v2->vec.y) && (mval[1] > scr->v1->vec.y)){
					/* test: is ScreenArea enough big for splitting */
					short tsplit= testsplitpoint(scr, dir, fac);
					if(tsplit){
						split = tsplit;
						/* delete old line from previous ScreenArea */
						if(!first) scrarea_draw_splitpoint(sa, dir, fac);
						sa= scr;
						first= 1;
						break;
					}
				}
				scr= scr->next;
			}
		}
		
		if (first || (dir=='v' && mval[0]!=mvalo[0]) || (dir=='h' && mval[1]!=mvalo[1])) {
			if (!first) {
				scrarea_draw_splitpoint(sa, dir, fac);
			}

			if(dir=='h') {
				fac= mval[1]- (sa->v1->vec.y);
				fac/= sa->v2->vec.y- sa->v1->vec.y;
			} else {
				fac= mval[0]- sa->v1->vec.x;
				fac/= sa->v4->vec.x- sa->v1->vec.x;
			}

			split= testsplitpoint(sa, dir, fac);
			if (split) {
				scrarea_draw_splitpoint(sa, dir, fac);
			} else {
				ok= -1;
			}

			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			first= 0;			
		}
		
		event= extern_qread(&val);

		/* change direction of splitting between horizontal and vertical
		 * patch was offered by Guillaume */
		if(val && (event==TABKEY || event==MIDDLEMOUSE)) {
			scrarea_draw_splitpoint(sa, dir, fac);
			if(dir=='h') {
				dir='v';
				set_cursor(CURSOR_Y_MOVE);
			} else {
				dir='h';
				set_cursor(CURSOR_X_MOVE);
			}
			first= 1;
		}

		if(val && event==LEFTMOUSE) {
			if(dir=='h') {
				fac= split- (sa->v1->vec.y);
				fac/= sa->v2->vec.y- sa->v1->vec.y;
			}
			else {
				fac= split- sa->v1->vec.x;
				fac/= sa->v4->vec.x- sa->v1->vec.x;
			}
			ok= 1;
		}
		if(val && (event==ESCKEY || event==RIGHTMOUSE)) {
			ok= -1;
		}
		bglFlush();
	}

	if (!first) {
		scrarea_draw_splitpoint(sa, dir, fac);
		bglFlush();
	}
	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_BACK);

	if(ok==1) {
		splitarea(sa, dir, fac);
		mainqenter(DRAWEDGES, 1);
		dodrawscreen= 1;		/* patch! event gets lost,,,? */
	}
}

View3D *find_biggest_view3d(void)
{
	ScrArea *sa= find_biggest_area_of_type(SPACE_VIEW3D);
	
	if (sa) {
		return (View3D*) sa->spacedata.first;
	} else {
		return NULL;
	}
}

ScrArea *find_biggest_area_of_type(int spacecode)
{
	ScrArea *sa, *biggest= NULL;
	int bigsize= 0;
	
	for (sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		if (spacecode==0 || sa->spacetype==spacecode) {
			int x= sa->v3->vec.x - sa->v1->vec.x;
			int y= sa->v3->vec.y - sa->v1->vec.y;
			int size= x*x + y*y;
		
			if (!biggest || size>bigsize) {
				biggest= sa;
				bigsize= size;
			}
		}
	}
	
	return biggest;
}

ScrArea *find_biggest_area(void)
{
	return find_biggest_area_of_type(0);
}

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
		sv->flag= 0;
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

void test_scale_screen(bScreen *sc)
/* test if screen vertices should be scaled */
/* also check offset */
{
	ScrVert *sv=0;
	ScrEdge *se;
	ScrArea *sa, *san;
	int yval;
	float facx, facy, tempf, min[2], max[2];

	sc->startx= prefstax;
	sc->starty= prefstay;
	sc->endx= prefstax+prefsizx-1;
	sc->endy= prefstay+prefsizy-1;

	/* calculate size */
	sv= sc->vertbase.first;
	min[0]= min[1]= 0.0;
	max[0]= sc->sizex;
	max[1]= sc->sizey;
	while(sv) {
		min[0]= MIN2(min[0], sv->vec.x);
		min[1]= MIN2(min[1], sv->vec.y);
		max[0]= MAX2(max[0], sv->vec.x);
		max[1]= MAX2(max[1], sv->vec.y);
		sv= sv->next;
	}

	/* always make 0.0 left under */
	sv= sc->vertbase.first;
	while(sv) {
		sv->vec.x -= min[0];
		sv->vec.y -= min[1];
		sv= sv->next;
	}
	

	sc->sizex= max[0]-min[0];
	sc->sizey= max[1]-min[1];

	if(sc->sizex!= prefsizx || sc->sizey!= prefsizy) {
		facx= prefsizx;
		facx/= (float)sc->sizex;
		facy= prefsizy;
		facy/= (float)sc->sizey;

		/* make sure it fits! */
		sv= sc->vertbase.first;
		while(sv) {
			tempf= ((float)sv->vec.x)*facx;
			sv->vec.x= (short)(tempf+0.5);
			sv->vec.x+= AREAGRID-1;
			sv->vec.x-=  (sv->vec.x % AREAGRID); 
			
			CLAMP(sv->vec.x, 0, prefsizx);

			tempf= ((float)sv->vec.y )*facy;
			sv->vec.y= (short)(tempf+0.5);
			sv->vec.y+= AREAGRID-1;
			sv->vec.y-=  (sv->vec.y % AREAGRID); 
			
			CLAMP(sv->vec.y, 0, prefsizy);

			sv= sv->next;
		}
		
		sc->sizex= prefsizx;
		sc->sizey= prefsizy;
	}

	/* test for collapsed areas. This could happen in some blender version... */
	sa= sc->areabase.first;
	while(sa) {
		san= sa->next;
		if(sa->v1==sa->v2 || sa->v3==sa->v4 || sa->v2==sa->v3) {
			del_area(sa);
			BLI_remlink(&sc->areabase, sa);
			MEM_freeN(sa);
		}
		sa= san;
	}

	/* make each window at least HEADERY high */
	sa= sc->areabase.first;
	while(sa) {

		if(sa->v1->vec.y+HEADERY > sa->v2->vec.y) {
			/* lower edge */
			se= screen_findedge(sc, sa->v4, sa->v1);
			if(se && sa->v1!=sa->v2 ) {
				select_connected_scredge(sc, se);
				
				/* all selected vertices get the right offset */
				yval= sa->v2->vec.y-HEADERY;
				sv= sc->vertbase.first;
				while(sv) {
					/* if is a collapsed area */
					if(sv!=sa->v2 && sv!=sa->v3) {
						if(sv->flag) sv->vec.y= yval;
					}
					sv= sv->next;
				}
			}
		}

		sa= sa->next;
	}

}

static void draw_front_xor_dirdist_line(char dir, int dist, int start, int end)
{
	if (dir=='h') {
		sdrawXORline(start, dist, end, dist);
		sdrawXORline(start, dist+1, end, dist+1);
	} else {
		sdrawXORline(dist, start, dist, end);
		sdrawXORline(dist+1, start, dist+1, end);
	}
}

static void moveareas(ScrEdge *edge)
{
	ScrVert *v1;
	ScrArea *sa;
	short mvalo[2], mval_prev=-1;
	short edge_start, edge_end, edge_position;
	short bigger, smaller, headery, areaminy;
	int delta, doit;
	char dir;
	
	if(edge->border) return;

	dir= scredge_is_horizontal(edge)?'h':'v';
	
	select_connected_scredge(G.curscreen, edge);

	edge_position= (dir=='h')?edge->v1->vec.y:edge->v1->vec.x;
	edge_start= 10000;
	edge_end= -10000;
	for (v1= G.curscreen->vertbase.first; v1; v1= v1->next) {
		if (v1->flag) {
			if (dir=='h') {
				edge_start= MIN2(edge_start, v1->vec.x);
				edge_end= MAX2(edge_end, v1->vec.x);
			} else {
				edge_start= MIN2(edge_start, v1->vec.y);
				edge_end= MAX2(edge_end, v1->vec.y);
			}
		}
	}

	/* now all verices with 'flag==1' are the ones that can be moved. */
	/* we check all areas and test for free space with MINSIZE */
	bigger= smaller= 10000;
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(dir=='h') {	/* if top or down edge selected, test height */
			if(sa->headertype) {
				headery= HEADERY;
				areaminy= AREAMINY;
			}
			else {
				headery= 0;
				areaminy= AREAMINY; /*areaminy= EDGEWIDTH;*/
			}

			if(sa->v1->flag && sa->v4->flag) {
				int y1;
				if(sa->v2->vec.y==G.curscreen->sizey-1)	/* top edge */
					y1= sa->v2->vec.y - sa->v1->vec.y-headery-EDGEWIDTH;
				else 
					y1= sa->v2->vec.y - sa->v1->vec.y-areaminy;
				bigger= MIN2(bigger, y1);
			}
			else if(sa->v2->flag && sa->v3->flag) {
				int y1;
				if(sa->v1->vec.y==0)	/* bottom edge */
					y1= sa->v2->vec.y - sa->v1->vec.y-headery-EDGEWIDTH;
				else
					y1= sa->v2->vec.y - sa->v1->vec.y-areaminy;
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
		sa= sa->next;
	}
	
	mywinset(G.curscreen->mainwin);

	glReadBuffer(GL_FRONT);
	glDrawBuffer(GL_FRONT);

	doit= delta= 0;
	getmouseco_sc(mvalo);
	draw_front_xor_dirdist_line(dir, edge_position+delta, edge_start, edge_end);

	while (!doit) {
		short val;
		unsigned short event= extern_qread(&val);
		
		if (event==MOUSEY) {
			short mval[2];
			
			getmouseco_sc(mval);
			if ((dir=='h' && mval_prev != mval[1]) || (dir=='v' && mval_prev != mval[0])) {
				/* update the previous val with this one for comparison next loop */
				if (dir=='h')	mval_prev = mval[1];
				else			mval_prev = mval[0];
				
				draw_front_xor_dirdist_line(dir, edge_position+delta, edge_start, edge_end);

				delta= (dir=='h')?(mval[1]-mvalo[1]):(mval[0]-mvalo[0]);
				delta= CLAMPIS(delta, -smaller, bigger);
				draw_front_xor_dirdist_line(dir, edge_position+delta, edge_start, edge_end);
				bglFlush();
			}
		} 
		else if (event==LEFTMOUSE) {
			doit= 1;
		} 
		else if (val) {
			if (ELEM(event, ESCKEY, RIGHTMOUSE))
				doit= -1;
			else if (ELEM(event, SPACEKEY, RETKEY))
				doit= 1;
		}
		else BIF_wait_for_statechange();
		
	}
	draw_front_xor_dirdist_line(dir, edge_position+delta, edge_start, edge_end);
	bglFlush();
	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_BACK);

	if (doit==1) {
		for (v1= G.curscreen->vertbase.first; v1; v1= v1->next) {
			if (v1->flag) {
					/* that way a nice AREAGRID  */
				if((dir=='v') && v1->vec.x>0 && v1->vec.x<G.curscreen->sizex-1) {
					v1->vec.x+= delta;
					if(delta != bigger && delta != -smaller) v1->vec.x-= (v1->vec.x % AREAGRID);
				}
				if((dir=='h') && v1->vec.y>0 && v1->vec.y<G.curscreen->sizey-1) {
					v1->vec.y+= delta;
					
					v1->vec.y+= AREAGRID-1;
					v1->vec.y-= (v1->vec.y % AREAGRID);
					
					/* prevent too small top header */
					if(v1->vec.y > G.curscreen->sizey-HEADERY)
						v1->vec.y= G.curscreen->sizey-HEADERY;
				}
			}
			v1->flag= 0;
		}
		
		removedouble_scrverts();
		removedouble_scredges();
		testareas();
	}
	
	mainqenter(DRAWEDGES, 1);
	dodrawscreen= 1;		/* patch! event gets lost,,,? */
}

static void scrollheader(ScrArea *area)
{
	short mval[2], mvalo[2];
	
	if(area->headbutlen<area->winx) {
		area->headbutofs= 0;
	}
	else if(area->headbutofs+area->winx > area->headbutlen) {
		area->headbutofs= area->headbutlen-area->winx;
	}

	getmouseco_sc(mvalo);

	while(get_mbut() & M_MOUSE) {
		getmouseco_sc(mval);
		if(mval[0]!=mvalo[0]) {
			area->headbutofs-= (mval[0]-mvalo[0]);

			if(area->headbutlen-area->winx < area->headbutofs) area->headbutofs= area->headbutlen-area->winx;
			if(area->headbutofs<0) area->headbutofs= 0;

			scrarea_do_headchange(area);
			scrarea_do_headdraw(area);

			screen_swapbuffers();
			
			mvalo[0]= mval[0];
		} else {
			BIF_wait_for_statechange();
		}
	}
}

int select_area(int spacetype)
{
	/* call from edit routines, when there are more areas
	 * of type 'spacetype', you can indicate an area manually
	 */
	ScrArea *sa, *sact = NULL;
	int tot=0;
	unsigned short event = 0;
	short val, mval[2];
	
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa->spacetype==spacetype) {
			sact= sa;
			tot++;
		}
		sa= sa->next;
	} 
	
	if(tot==0) {
		error("Can't do this! Open correct window");
		return 0;
	}
	
	if(tot==1) {
		if(curarea!=sact) areawinset(sact->win);
		return 1;
	}
	else if(tot>1) {
		set_cursor(CURSOR_HELP);
		while(1) {
			event= extern_qread(&val);
			
			if (val) {
				if(event==ESCKEY) break;
				if(event==LEFTMOUSE) break;
				if(event==SPACEKEY) break;
			} else {
				BIF_wait_for_statechange();
			}
		}
		screen_set_cursor(G.curscreen);
		
		/* recalculate winakt */
		getmouseco_sc(mval);
		
		if(event==LEFTMOUSE) {
			ScrArea *sa= screen_find_area_for_pt(G.curscreen, mval);
			
			if (sa &&sa->spacetype==spacetype) {
				G.curscreen->winakt= sa->win;
				areawinset(G.curscreen->winakt);
			} else {
				error("Wrong window");
				return 0;
			}
		}
	}
	
	if(event==LEFTMOUSE) return 1;
	else return 0;
}

/* ************  END JOIN/SPLIT/MOVE ************** */
/* **************** DRAW SCREENEDGES ***************** */


void draw_area_emboss(ScrArea *sa)
{
	
	/* set transp line */
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	/* right  */
	glColor4ub(0,0,0, 50);
	sdrawline(sa->winx-1, 0, sa->winx-1, sa->winy-1);
	
	/* bottom  */
	if(sa->headertype!=HEADERDOWN) {
		glColor4ub(0,0,0, 80);
		sdrawline(0, 0, sa->winx-1, 0);
	}
	
	/* top  */
	if(sa->headertype!=HEADERTOP) {
		glColor4ub(255,255,255, 60);
		sdrawline(0, sa->winy-1, sa->winx-1, sa->winy-1);
	}
	/* left  */
	glColor4ub(255,255,255, 50);
	sdrawline(0, 0, 0, sa->winy);

	glDisable( GL_BLEND );
}


void drawscredge_area(ScrArea *sa)
{
	short x1= sa->v1->vec.x;
	short y1= sa->v1->vec.y;
	short x2= sa->v3->vec.x;
	short y2= sa->v3->vec.y;

	/* this to fill the (undrawn) edge area with back color first */
	glColor3f(SCR_BACK,SCR_BACK,SCR_BACK);
	sdrawline(x2, y1, x2, y2);
	sdrawline(x1, y1, x2, y1);
	
	cpack(0x0);
	
	/* Simple hack to make sure round corners arntdrawn with the minimal theme,
	 * Nothing wrong with it IMHO, but just be aware its used so the following
	 * if's never compare true with HEADERTOP or HEADERDOWN */
	if (BIF_GetThemeValue(TH_BUT_DRAWTYPE) == TH_MINIMAL)
		sa->headertype = -sa->headertype;
	
	/* right border area */
	if(sa->headertype==HEADERTOP) sdrawline(x2, y1, x2, y2-SCR_ROUND+1);
	else if(sa->headertype==HEADERDOWN) sdrawline(x2, y1+SCR_ROUND-1, x2, y2);
	else sdrawline(x2, y1, x2, y2);
	
	/* left border area */
	if(x1>0) { // otherwise it draws the emboss of window over
		if(sa->headertype==HEADERTOP) sdrawline(x1, y1, x1, y2-SCR_ROUND+1);
		else if(sa->headertype==HEADERDOWN) sdrawline(x1, y1+SCR_ROUND-1, x1, y2);
		else sdrawline(x1, y1, x1, y2);
	}	
	/* top border area */
	if(sa->headertype==HEADERTOP) sdrawline(x1+SCR_ROUND-3, y2, x2-SCR_ROUND+3, y2);
	else sdrawline(x1, y2, x2, y2);
	
	/* bottom border area */
	if(sa->headertype==HEADERDOWN) sdrawline(x1+SCR_ROUND-3, y1, x2-SCR_ROUND+3, y1);
	else sdrawline(x1, y1, x2, y1);
	
	/* restore real header type */
	if (BIF_GetThemeValue(TH_BUT_DRAWTYPE) == TH_MINIMAL)
		sa->headertype = -sa->headertype;
}

/* ********************************* */

/* for depgraph updating, all layers visible in a screen */
unsigned int screen_view3d_layers(void)
{
	ScrArea *sa;
	int layer= G.scene->lay;	/* as minimum this */
	
	for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa->spacetype==SPACE_VIEW3D)
			layer |= ((View3D *)sa->spacedata.first)->lay;
	}
	return layer;
}

bScreen *default_twosplit() 
{
	bScreen *sc= addscreen("screen");
	ScrArea *sa;

	splitarea( (ScrArea *)sc->areabase.first, 'h', 0.99);
	sa= sc->areabase.first;
	newspace(sa, SPACE_VIEW3D);
	newspace(sa->next, SPACE_INFO);
	
	return sc;
}

void initscreen(void)
{
	default_twosplit();
}

static int curcursor;

int get_cursor(void) {
	return curcursor;
}

void set_cursor(int curs) {
	if (G.background == 0) {
		if (curs!=curcursor) {
			curcursor= curs;
			window_set_cursor(mainwin, curs);
		}
	}
}

void unlink_screen(bScreen *sc) {
	ScrArea *sa;
	
	for (sa= sc->areabase.first; sa; sa= sa->next)	
		del_area(sa);
}

void warp_pointer(int x, int y)
{
	window_warp_pointer(mainwin, x, y);
}

void set_timecursor(int nr)
{
		/* 10 8x8 digits */
	static char number_bitmaps[10][8]= {
		{0,  56,  68,  68,  68,  68,  68,  56}, 
		{0,  24,  16,  16,  16,  16,  16,  56}, 
		{0,  60,  66,  32,  16,   8,   4, 126}, 
		{0, 124,  32,  16,  56,  64,  66,  60}, 
		{0,  32,  48,  40,  36, 126,  32,  32}, 
		{0, 124,   4,  60,  64,  64,  68,  56}, 
		{0,  56,   4,   4,  60,  68,  68,  56}, 
		{0, 124,  64,  32,  16,   8,   8,   8}, 
		{0,  60,  66,  66,  60,  66,  66,  60}, 
		{0,  56,  68,  68, 120,  64,  68,  56} 
	};
	unsigned char mask[16][2];
	unsigned char bitmap[16][2];
	int i, idx;

	memset(&bitmap, 0x00, sizeof(bitmap));
	memset(&mask, 0xFF, sizeof(mask));
	
		/* print number bottom right justified */
	for (idx= 3; nr && idx>=0; idx--) {
		char *digit= number_bitmaps[nr%10];
		int x = idx%2;
		int y = idx/2;
		
		for (i=0; i<8; i++)
			bitmap[i + y*8][x]= digit[i];
		nr/= 10;
	}

	curcursor= CURSOR_NONE;
	window_set_custom_cursor(mainwin, mask, bitmap, 7, 7);
	BIF_renderwin_set_custom_cursor(mask, bitmap);
}
