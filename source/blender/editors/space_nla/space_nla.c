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

#include "BKE_colortools.h"
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

#include "nla_intern.h"	// own include

/* ******************** default callbacks for nla space ***************** */

static SpaceLink *nla_new(const bContext *C)
{
	ARegion *ar;
	SpaceNla *snla;
	
	snla= MEM_callocN(sizeof(SpaceNla), "initnla");
	snla->spacetype= SPACE_NLA;
	
	/* allocate DopeSheet data for NLA Editor */
	snla->ads= MEM_callocN(sizeof(bDopeSheet), "NLAEdit DopeSheet");
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for nla");
	
	BLI_addtail(&snla->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* channel list region XXX */
	ar= MEM_callocN(sizeof(ARegion), "area region from do_versions");
	BLI_addtail(&snla->regionbase, ar);
	ar->regiontype= RGN_TYPE_CHANNELS;
	ar->alignment= RGN_ALIGN_LEFT;
	
	ar->v2d.scroll = (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM);
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for nla");
	
	BLI_addtail(&snla->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	ar->v2d.tot.xmin= 1.0f;
	ar->v2d.tot.ymin=	0.0f;
	ar->v2d.tot.xmax= 1000.0f;
	ar->v2d.tot.ymax= 1000.0f;
	
	ar->v2d.cur.xmin= -5.0f;
	ar->v2d.cur.ymin= 0.0f;
	ar->v2d.cur.xmax= 65.0f;
	ar->v2d.cur.ymax= 1000.0f;
	
	ar->v2d.min[0]= 0.0f;
	ar->v2d.min[1]= 0.0f;
	
	ar->v2d.max[0]= MAXFRAMEF;
	ar->v2d.max[1]= 1000.0f;
	
	ar->v2d.minzoom= 0.1f;
	ar->v2d.maxzoom= 50.0f;
	
	ar->v2d.scroll |= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
	ar->v2d.scroll |= (V2D_SCROLL_RIGHT);
	ar->v2d.keepzoom= V2D_LOCKZOOM_Y;
		
	
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

}

static SpaceLink *nla_duplicate(SpaceLink *sl)
{
	SpaceNla *snlan= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	
	return (SpaceLink *)snlan;
}

static void nla_channel_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	// SpaceNla *snla= (SpaceNla*)CTX_wm_space_data(C);
	// View2D *v2d= &ar->v2d;
	float col[3];
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	// UI_view2d_view_ortho(C, v2d);
	
	/* data... */
	
	
	/* reset view matrix */
	//UI_view2d_view_restore(C);
	
	/* scrollers? */
}


/* add handlers, stuff you only do once or on area/region changes */
static void nla_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	ListBase *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_listbase(wm, "NLA", SPACE_NLA, 0);	/* XXX weak? */
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void nla_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	// SpaceNla *snla= (SpaceNla*)CTX_wm_space_data(C);
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

void nla_operatortypes(void)
{
	
}

void nla_keymap(struct wmWindowManager *wm)
{
	
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

static void nla_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
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
	st->keymap= nla_keymap;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype nla region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= nla_main_area_init;
	art->draw= nla_main_area_draw;
	art->listener= nla_main_area_listener;
	art->keymapflag= ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES;

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
	
	//art->init= nla_channel_area_init;
	art->draw= nla_channel_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	
	BKE_spacetype_register(st);
}

