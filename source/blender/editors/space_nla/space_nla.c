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

#include "DNA_anim_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_nla.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "ED_anim_api.h"
#include "ED_markers.h"
#include "ED_space_api.h"
#include "ED_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "nla_intern.h"	// own include

/* ******************** manage regions ********************* */

ARegion *nla_has_buttons_region(ScrArea *sa)
{
	ARegion *ar, *arnew;
	
	for (ar= sa->regionbase.first; ar; ar= ar->next) {
		if (ar->regiontype==RGN_TYPE_UI)
			return ar;
	}
	
	/* add subdiv level; after main */
	for (ar= sa->regionbase.first; ar; ar= ar->next) {
		if (ar->regiontype==RGN_TYPE_WINDOW)
			break;
	}
	
	/* is error! */
	if (ar==NULL) return NULL;
	
	arnew= MEM_callocN(sizeof(ARegion), "buttons for nla");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype= RGN_TYPE_UI;
	arnew->alignment= RGN_ALIGN_RIGHT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}



/* ******************** default callbacks for nla space ***************** */

static SpaceLink *nla_new(const bContext *C)
{
	Scene *scene= CTX_data_scene(C);
	ARegion *ar;
	SpaceNla *snla;
	
	snla= MEM_callocN(sizeof(SpaceNla), "initnla");
	snla->spacetype= SPACE_NLA;
	
	/* allocate DopeSheet data for NLA Editor */
	snla->ads= MEM_callocN(sizeof(bDopeSheet), "NlaEdit DopeSheet");
	
	/* set auto-snapping settings */
	snla->autosnap = SACTSNAP_FRAME;
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for nla");
	
	BLI_addtail(&snla->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* channel list region */
	ar= MEM_callocN(sizeof(ARegion), "channel list for nla");
	BLI_addtail(&snla->regionbase, ar);
	ar->regiontype= RGN_TYPE_CHANNELS;
	ar->alignment= RGN_ALIGN_LEFT;
	
		/* only need to set these settings since this will use the 'stack' configuration */
	ar->v2d.scroll = V2D_SCROLL_BOTTOM;
	ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;
	
	/* ui buttons */
	ar= MEM_callocN(sizeof(ARegion), "buttons area for nla");
	
	BLI_addtail(&snla->regionbase, ar);
	ar->regiontype= RGN_TYPE_UI;
	ar->alignment= RGN_ALIGN_RIGHT;
	ar->flag = RGN_FLAG_HIDDEN;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for nla");
	
	BLI_addtail(&snla->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	ar->v2d.tot.xmin= (float)(SFRA-10);
	ar->v2d.tot.ymin= -500.0f;
	ar->v2d.tot.xmax= (float)(EFRA+10);
	ar->v2d.tot.ymax= 0.0f;
	
	ar->v2d.cur = ar->v2d.tot;
	
	ar->v2d.min[0]= 0.0f;
 	ar->v2d.min[1]= 0.0f;
	
	ar->v2d.max[0]= MAXFRAMEF;
 	ar->v2d.max[1]= 10000.0f;
 	
	ar->v2d.minzoom= 0.01f;
	ar->v2d.maxzoom= 50;
	ar->v2d.scroll = (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
	ar->v2d.scroll |= (V2D_SCROLL_RIGHT);
	ar->v2d.keepzoom= V2D_LOCKZOOM_Y;
	ar->v2d.align= V2D_ALIGN_NO_POS_Y;
	ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;
	
	return (SpaceLink *)snla;
}

/* not spacelink itself */
static void nla_free(SpaceLink *sl)
{	
	SpaceNla *snla= (SpaceNla*) sl;
	
	if (snla->ads) {
		BLI_freelistN(&snla->ads->chanbase);
		MEM_freeN(snla->ads);
	}
}


/* spacetype; init callback */
static void nla_init(struct wmWindowManager *wm, ScrArea *sa)
{
	SpaceNla *snla= (SpaceNla *)sa->spacedata.first;
	
	/* init dopesheet data if non-existant (i.e. for old files) */
	if (snla->ads == NULL)
		snla->ads= MEM_callocN(sizeof(bDopeSheet), "NlaEdit DopeSheet");

	ED_area_tag_refresh(sa);
}

static SpaceLink *nla_duplicate(SpaceLink *sl)
{
	SpaceNla *snlan= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	snlan->ads= MEM_dupallocN(snlan->ads);
	
	return (SpaceLink *)snlan;
}

/* add handlers, stuff you only do once or on area/region changes */
static void nla_channel_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);
	
	/* own keymap */
	// TODO: cannot use generic copy, need special NLA version
	keymap= WM_keymap_find(wm->defaultconf, "NLA Channels", SPACE_NLA, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
	keymap= WM_keymap_find(wm->defaultconf, "NLA Generic", SPACE_NLA, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

/* draw entirely, view changes should be handled here */
static void nla_channel_area_draw(const bContext *C, ARegion *ar)
{
	SpaceNla *snla= CTX_wm_space_nla(C);
	bAnimContext ac;
	View2D *v2d= &ar->v2d;
	View2DScrollers *scrollers;
	float col[3];
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(C, v2d);
	
	/* data */
	if (ANIM_animdata_get_context(C, &ac)) {
		draw_nla_channel_list((bContext *)C, &ac, snla, ar);
	}
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers= UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}


/* add handlers, stuff you only do once or on area/region changes */
static void nla_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_find(wm->defaultconf, "NLA Data", SPACE_NLA, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
	keymap= WM_keymap_find(wm->defaultconf, "NLA Generic", SPACE_NLA, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void nla_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceNla *snla= CTX_wm_space_nla(C);
	bAnimContext ac;
	View2D *v2d= &ar->v2d;
	View2DGrid *grid;
	View2DScrollers *scrollers;
	float col[3];
	short unit=0, flag=0;
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(C, v2d);
	
	/* time grid */
	unit= (snla->flag & SNLA_DRAWTIME)? V2D_UNIT_SECONDS : V2D_UNIT_FRAMES;
	grid= UI_view2d_grid_calc(C, v2d, unit, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY, ar->winx, ar->winy);
	UI_view2d_grid_draw(C, v2d, grid, V2D_GRIDLINES_ALL);
	UI_view2d_grid_free(grid);
	
	/* data */
	if (ANIM_animdata_get_context(C, &ac)) {
		/* strips and backdrops */
		draw_nla_main_data(&ac, snla, ar);
		
		/* text draw cached, in pixelspace now */
		UI_view2d_text_cache_draw(ar);
	}
	
	UI_view2d_view_ortho(C, v2d);
	
	/* current frame */
	if (snla->flag & SNLA_DRAWTIME) 	flag |= DRAWCFRA_UNIT_SECONDS;
	if ((snla->flag & SNLA_NODRAWCFRANUM)==0)  flag |= DRAWCFRA_SHOW_NUMBOX;
	ANIM_draw_cfra(C, v2d, flag);
	
	/* markers */
	UI_view2d_view_orthoSpecial(C, v2d, 1);
	draw_markers_time(C, 0);
	
	/* preview range */
	UI_view2d_view_ortho(C, v2d);
	ANIM_draw_previewrange(C, v2d);
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers= UI_view2d_scrollers_calc(C, v2d, unit, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}


/* add handlers, stuff you only do once or on area/region changes */
static void nla_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);
}

static void nla_header_area_draw(const bContext *C, ARegion *ar)
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
	
	nla_header_buttons(C, ar);
	
	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

/* add handlers, stuff you only do once or on area/region changes */
static void nla_buttons_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	ED_region_panels_init(wm, ar);
	
	keymap= WM_keymap_find(wm->defaultconf, "NLA Generic", SPACE_NLA, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void nla_buttons_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, NULL, -1);
}

static void nla_region_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_ANIMATION:
			ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			switch(wmn->data) {
				case ND_OB_ACTIVE:
				case ND_FRAME:
				case ND_MARKERS:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_OBJECT:
			switch(wmn->data) {
				case ND_BONE_ACTIVE:
				case ND_BONE_SELECT:
				case ND_KEYS:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		default:
			if(wmn->data==ND_KEYS)
				ED_region_tag_redraw(ar);
			break;
	}
}


static void nla_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_ANIMATION:
			ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			switch(wmn->data) {
				case ND_RENDER_OPTIONS:
				case ND_OB_ACTIVE:
				case ND_FRAME:
				case ND_MARKERS:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_OBJECT:
			switch(wmn->data) {
				case ND_BONE_ACTIVE:
				case ND_BONE_SELECT:
				case ND_KEYS:
				case ND_TRANSFORM:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		default:
			if(wmn->data==ND_KEYS)
				ED_region_tag_redraw(ar);
	}
}

static void nla_channel_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_ANIMATION:
			ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			switch(wmn->data) {
				case ND_OB_ACTIVE:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_OBJECT:
			switch(wmn->data) {
				case ND_BONE_ACTIVE:
				case ND_BONE_SELECT:
				case ND_KEYS:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		default:
			if(wmn->data==ND_KEYS)
				ED_region_tag_redraw(ar);
	}
}

/* editor level listener */
static void nla_listener(ScrArea *sa, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_ANIMATION:
			// TODO: filter specific types of changes?
			ED_area_tag_refresh(sa);
			break;
		case NC_SCENE:
			/*switch (wmn->data) {
				case ND_OB_ACTIVE:
				case ND_OB_SELECT:
					ED_area_tag_refresh(sa);
					break;
			}*/
			ED_area_tag_refresh(sa);
			break;
		case NC_OBJECT:
			/*switch (wmn->data) {
				case ND_BONE_SELECT:
				case ND_BONE_ACTIVE:
					ED_area_tag_refresh(sa);
					break;
			}*/
			ED_area_tag_refresh(sa);
			break;
		case NC_SPACE:
			if(wmn->data == ND_SPACE_NLA)
				ED_area_tag_redraw(sa);
			break;
	}
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_nla(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype nla");
	ARegionType *art;
	
	st->spaceid= SPACE_NLA;
	
	st->new= nla_new;
	st->free= nla_free;
	st->init= nla_init;
	st->duplicate= nla_duplicate;
	st->operatortypes= nla_operatortypes;
	st->listener= nla_listener;
	st->keymap= nla_keymap;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype nla region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= nla_main_area_init;
	art->draw= nla_main_area_draw;
	art->listener= nla_main_area_listener;
	art->keymapflag= ED_KEYMAP_VIEW2D/*|ED_KEYMAP_MARKERS*/|ED_KEYMAP_ANIMATION|ED_KEYMAP_FRAMES;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype nla region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
	art->init= nla_header_area_init;
	art->draw= nla_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: channels */
	art= MEM_callocN(sizeof(ARegionType), "spacetype nla region");
	art->regionid = RGN_TYPE_CHANNELS;
	art->minsizex= 200;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
	art->init= nla_channel_area_init;
	art->draw= nla_channel_area_draw;
	art->listener= nla_channel_area_listener;
	
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: UI buttons */
	art= MEM_callocN(sizeof(ARegionType), "spacetype nla region");
	art->regionid = RGN_TYPE_UI;
	art->minsizex= 200;
	art->keymapflag= ED_KEYMAP_UI;
	art->listener= nla_region_listener;
	art->init= nla_buttons_area_init;
	art->draw= nla_buttons_area_draw;
	
	BLI_addhead(&st->regiontypes, art);

	nla_buttons_register(art);
	
	
	BKE_spacetype_register(st);
}

