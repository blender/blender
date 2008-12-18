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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_markers.h"

#include "ipo_intern.h"	// own include

/* ******************** default callbacks for ipo space ***************** */

static SpaceLink *ipo_new(void)
{
	ARegion *ar;
	SpaceIpo *sipo;
	
	sipo= MEM_callocN(sizeof(SpaceIpo), "initipo");
	sipo->spacetype= SPACE_IPO;
	sipo->blocktype= ID_OB;
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for ipo");
	
	BLI_addtail(&sipo->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* channels */
	ar= MEM_callocN(sizeof(ARegion), "main area for ipo");
	
	BLI_addtail(&sipo->regionbase, ar);
	ar->regiontype= RGN_TYPE_CHANNELS;
	ar->alignment= RGN_ALIGN_LEFT;
	
	ar->v2d.scroll = (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM);
	
	/* XXX view2d init for channels */
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for ipo");
	
	BLI_addtail(&sipo->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	ar->v2d.tot.xmin= 0.0f;
	ar->v2d.tot.ymin= -10.0f;
	ar->v2d.tot.xmax= 250.0;
	ar->v2d.tot.ymax= 10.0f;
	
	ar->v2d.cur= ar->v2d.tot;
	
	ar->v2d.min[0]= 0.01f;
	ar->v2d.min[1]= 0.01f;
	
	ar->v2d.max[0]= MAXFRAMEF;
	ar->v2d.max[1]= 50000.0f;
	
	ar->v2d.scroll= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
	ar->v2d.scroll |= (V2D_SCROLL_LEFT|V2D_SCROLL_SCALE_VERTICAL);
	
	ar->v2d.keeptot= 0;
	
	/* channel list region XXX */

	
	return (SpaceLink *)sipo;
}

/* not spacelink itself */
static void ipo_free(SpaceLink *sl)
{	
	SpaceIpo *si= (SpaceIpo*) sl;
	
	if(si->editipo) MEM_freeN(si->editipo);
	// XXX free_ipokey(&si->ipokey);

}


/* spacetype; init callback */
static void ipo_init(struct wmWindowManager *wm, ScrArea *sa)
{

}

static SpaceLink *ipo_duplicate(SpaceLink *sl)
{
	SpaceIpo *sipon= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	sipon->editipo= NULL;
	sipon->ipokey.first= sipon->ipokey.last= NULL;
	
	return (SpaceLink *)sipon;
}


// XXX this should be defined in some general lib for anim editors...
static void draw_cfra(const bContext *C, SpaceIpo *sipo, View2D *v2d)
{
	Scene *scene= CTX_data_scene(C);
	float vec[2];
	
	//vec[0] = get_ipo_cfra_from_cfra(sipo, scene->r.cfra);
	vec[0] = scene->r.cfra;
	vec[0]*= scene->r.framelen;
	
	vec[1]= v2d->cur.ymin;
	UI_ThemeColor(TH_CFRAME);
	glLineWidth(2.0);
	
	glBegin(GL_LINE_STRIP);
	glVertex2fv(vec);
	vec[1]= v2d->cur.ymax;
	glVertex2fv(vec);
	glEnd();
	
#if 0
	if(sipo->blocktype==ID_OB) {
		ob= (scene->basact) ? (scene->basact->object) : 0;
		if (ob && (ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)!=0.0)) { 
			vec[0]-= give_timeoffset(ob);
			
			UI_ThemeColorShade(TH_HILITE, -30);
			
			glBegin(GL_LINE_STRIP);
			glVertex2fv(vec);
			vec[1]= G.v2d->cur.ymin;
			glVertex2fv(vec);
			glEnd();
		}
	}
#endif
	
	glLineWidth(1.0);
	
	/* Draw current frame number in a little box */
	//draw_cfra_number(vec[0]);
}

/* add handlers, stuff you only do once or on area/region changes */
static void ipo_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	ListBase *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_listbase(wm, "Ipo", SPACE_IPO, 0);	/* XXX weak? */
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void ipo_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceIpo *sipo= (SpaceIpo*)CTX_wm_space_data(C);
	View2D *v2d= &ar->v2d;
	View2DGrid *grid;
	View2DScrollers *scrollers;
	float col[3];
	int unit;
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(C, v2d);
	
	/* grid */
	unit= (sipo->flag & SIPO_DRAWTIME)? V2D_UNIT_SECONDS : V2D_UNIT_FRAMES;
	grid= UI_view2d_grid_calc(C, v2d, unit, V2D_GRID_NOCLAMP, V2D_UNIT_VALUES/*unit-y*/, V2D_GRID_NOCLAMP, ar->winx, ar->winy);
	UI_view2d_grid_draw(C, v2d, grid, V2D_GRIDLINES_ALL);
	UI_view2d_grid_free(grid);
	
	/* data... */
	
	/* current frame */
	draw_cfra(C, sipo, v2d);
	
	/* markers */
	UI_view2d_view_orthoSpecial(C, v2d, 1);
	draw_markers_time(C, 0);
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
		// FIXME: args for scrollers depend on the type of data being shown...
	scrollers= UI_view2d_scrollers_calc(C, v2d, unit, V2D_GRID_NOCLAMP, V2D_UNIT_VALUES/*unit-y*/, V2D_GRID_NOCLAMP);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

void ipo_operatortypes(void)
{
}

void ipo_keymap(struct wmWindowManager *wm)
{
}

static void ipo_channel_area_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);
}

static void ipo_channel_area_draw(const bContext *C, ARegion *ar)
{
	//SpaceIpo *sipo= C->area->spacedata.first;
	View2D *v2d= &ar->v2d;
	View2DScrollers *scrollers;
	float col[3];
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_SHADE2, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(C, v2d);
	
	/* data... */
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers= UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

/* add handlers, stuff you only do once or on area/region changes */
static void ipo_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);
}

static void ipo_header_area_draw(const bContext *C, ARegion *ar)
{
	float col[3];
	
	/* clear */
	if(ED_screen_area_active(C))
		UI_GetThemeColor3fv(TH_HEADER, col);
	else
		UI_GetThemeColor3fv(TH_HEADERDESEL, col);
	
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	/* set view2d view matrix for scrolling (without scrollers) */
	UI_view2d_view_ortho(C, &ar->v2d);
	
	ipo_header_buttons(C, ar);
	
	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

static void ipo_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->type) {
		
		case WM_NOTE_MARKERS_CHANGED:
			ED_region_tag_redraw(ar);
			break;
	}
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_ipo(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype ipo");
	ARegionType *art;
	
	st->spaceid= SPACE_IPO;
	
	st->new= ipo_new;
	st->free= ipo_free;
	st->init= ipo_init;
	st->duplicate= ipo_duplicate;
	st->operatortypes= ipo_operatortypes;
	st->keymap= ipo_keymap;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype ipo region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= ipo_main_area_init;
	art->draw= ipo_main_area_draw;
	art->listener= ipo_main_area_listener;
	art->keymapflag= ED_KEYMAP_VIEW2D|ED_KEYMAP_MARKERS;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype ipo region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
	art->init= ipo_header_area_init;
	art->draw= ipo_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: channels */
	art= MEM_callocN(sizeof(ARegionType), "spacetype ipo region");
	art->regionid = RGN_TYPE_CHANNELS;
	art->minsizex= 200;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
	art->init= ipo_channel_area_init;
	art->draw= ipo_channel_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	
	BKE_spacetype_register(st);
}

