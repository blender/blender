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

#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_context.h"
#include "BKE_colortools.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "BIF_gl.h"
#include "BLF_api.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_markers.h"

#include "info_intern.h"	// own include

/* ******************** default callbacks for info space ***************** */

static SpaceLink *info_new(const bContext *C)
{
	ARegion *ar;
	SpaceInfo *sinfo;
	
	sinfo= MEM_callocN(sizeof(SpaceInfo), "initinfo");
	sinfo->spacetype= SPACE_INFO;
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for info");
	
	BLI_addtail(&sinfo->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for info");
	
	BLI_addtail(&sinfo->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	/* channel list region XXX */

	
	return (SpaceLink *)sinfo;
}

/* not spacelink itself */
static void info_free(SpaceLink *sl)
{	
//	SpaceInfo *sinfo= (SpaceInfo*) sl;
	
}


/* spacetype; init callback */
static void info_init(struct wmWindowManager *wm, ScrArea *sa)
{

}

static SpaceLink *info_duplicate(SpaceLink *sl)
{
	SpaceInfo *sinfon= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	
	return (SpaceLink *)sinfon;
}



/* add handlers, stuff you only do once or on area/region changes */
static void info_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	ListBase *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_STANDARD, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_listbase(wm, "info", SPACE_INFO, 0);	/* XXX weak? */
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void info_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	// SpaceInfo *sinfo= (SpaceInfo*)CTX_wm_space_data(C);
	View2D *v2d= &ar->v2d;
	float col[3];
	float width, height;

	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(C, v2d);
		
	/* data... */
	// XXX 2.50 Testing new font library - Diego
	glColor3f(1.0, 0.0, 0.0);
	BLF_aspect(1.0);

	BLF_size(14, 96);
	BLF_position(5.0, 5.0, 0.0);

	width= BLF_width("Hello Blender, size 14, dpi 96");
	height= BLF_height("Hello Blender, size 14, dpi 96");

	glRectf(7.0, 20.0, 7.0+width, 20.0+height);
	glRectf(5.0+width+10.0, 3.0, 5.0+width+10.0+width, 3.0+height);
	BLF_draw("Hello Blender, size 14, dpi 96");

	glColor3f(0.0, 0.0, 1.0);
	BLF_size(11, 96);
	BLF_position(200.0, 50.0, 0.0);
	BLF_rotation(45.0f);
	BLF_draw("Another Hello Blender, size 11 and dpi 96!!");

	glColor3f(0.8, 0.0, 0.7);
	BLF_size(12, 72);
	BLF_position(200.0, 100.0, 0.0);
	BLF_rotation(180.0f);
	BLF_draw("Hello World, size 12, dpi 72");
	
	glColor3f(0.8, 0.7, 0.5);
	BLF_size(12, 96);
	BLF_position(5.0, 200.0, 0.0);
	BLF_rotation(0.0f);
	BLF_draw("And this make a new glyph cache!!");

	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers? */
}

void info_operatortypes(void)
{
	
}

void info_keymap(struct wmWindowManager *wm)
{
	
}

/* add handlers, stuff you only do once or on area/region changes */
static void info_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);
}

static void info_header_area_draw(const bContext *C, ARegion *ar)
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
	
	info_header_buttons(C, ar);
	
	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

static void info_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
}

static void info_header_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_SCREEN:
			if(wmn->data==ND_SCREENCAST)
				ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			if(wmn->data==ND_RENDER_RESULT)
				ED_region_tag_redraw(ar);
			break;
	}
	
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_info(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype info");
	ARegionType *art;
	
	st->spaceid= SPACE_INFO;
	
	st->new= info_new;
	st->free= info_free;
	st->init= info_init;
	st->duplicate= info_duplicate;
	st->operatortypes= info_operatortypes;
	st->keymap= info_keymap;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype info region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= info_main_area_init;
	art->draw= info_main_area_draw;
	art->listener= info_main_area_listener;
	art->keymapflag= ED_KEYMAP_VIEW2D;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype info region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	art->listener= info_header_listener;
	art->init= info_header_area_init;
	art->draw= info_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	
	BKE_spacetype_register(st);
}

