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

#include "DNA_action_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_colortools.h"
#include "BKE_global.h"
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

#include "action_intern.h"	// own include

/* ******************** default callbacks for action space ***************** */

static SpaceLink *action_new(void)
{
	ARegion *ar;
	SpaceAction *saction;
	
	saction= MEM_callocN(sizeof(SpaceAction), "initaction");
	saction->spacetype= SPACE_ACTION;
	saction->autosnap = SACTSNAP_FRAME;
	
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for action");
	
	BLI_addtail(&saction->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for action");
	
	BLI_addtail(&saction->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	ar->v2d.tot.xmin= 1.0f;
	ar->v2d.tot.ymin= -1000.0f;
	ar->v2d.tot.xmax= 1000.0f;
	ar->v2d.tot.ymax= 0.0f;
	
	ar->v2d.cur.xmin= -5.0f;
	ar->v2d.cur.ymin= -75.0f;
	ar->v2d.cur.xmax= 65.0f;
	ar->v2d.cur.ymax= 5.0f;
	
	ar->v2d.min[0]= 0.0f;
 	ar->v2d.min[1]= 0.0f;
	
	ar->v2d.max[0]= MAXFRAMEF;
 	ar->v2d.max[1]= 1000.0f;
 	
	ar->v2d.minzoom= 0.01f;
	ar->v2d.maxzoom= 50;
	ar->v2d.scroll |= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
	ar->v2d.scroll |= (V2D_SCROLL_RIGHT);
	ar->v2d.keepzoom= V2D_LOCKZOOM_Y;
	ar->v2d.align= V2D_ALIGN_NO_POS_X;
	
	/* channel list region XXX */
	ar= MEM_callocN(sizeof(ARegion), "area region from do_versions");
	BLI_addtail(&saction->regionbase, ar);
	ar->regiontype= RGN_TYPE_CHANNELS;
	ar->alignment= RGN_ALIGN_LEFT;
				
	
	return (SpaceLink *)saction;
}

/* not spacelink itself */
static void action_free(SpaceLink *sl)
{	
//	SpaceAction *saction= (SpaceAction*) sl;
	
}


/* spacetype; init callback */
static void action_init(struct wmWindowManager *wm, ScrArea *sa)
{

}

static SpaceLink *action_duplicate(SpaceLink *sl)
{
	SpaceAction *sactionn= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	
	return (SpaceLink *)sactionn;
}



/* add handlers, stuff you only do once or on area/region changes */
static void action_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	ListBase *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_listbase(wm, "Action", SPACE_ACTION, 0);	/* XXX weak? */
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void action_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	// SpaceAction *saction= C->area->spacedata.first;
	View2D *v2d= &ar->v2d;
	float col[3];
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(C, v2d);
		
	/* data... */
	
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers? */
}

void action_operatortypes(void)
{
	
}

void action_keymap(struct wmWindowManager *wm)
{
	
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);
}

static void action_header_area_draw(const bContext *C, ARegion *ar)
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
	
	action_header_buttons(C, ar);
	
	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

static void action_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_action(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype action");
	ARegionType *art;
	
	st->spaceid= SPACE_ACTION;
	
	st->new= action_new;
	st->free= action_free;
	st->init= action_init;
	st->duplicate= action_duplicate;
	st->operatortypes= action_operatortypes;
	st->keymap= action_keymap;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype action region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= action_main_area_init;
	art->draw= action_main_area_draw;
	art->listener= action_main_area_listener;
	art->keymapflag= ED_KEYMAP_VIEW2D;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype action region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
	art->init= action_header_area_init;
	art->draw= action_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: channels */
	art= MEM_callocN(sizeof(ARegionType), "spacetype action region");
	art->regionid = RGN_TYPE_CHANNELS;
	art->minsizex = 200;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
	//art->init= action_channel_area_init;
	//art->draw= action_channel_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	
	BKE_spacetype_register(st);
}

