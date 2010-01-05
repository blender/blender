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

#include "DNA_sound_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
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

#include "sound_intern.h"	// own include

/* ******************** default callbacks for sound space ***************** */

static SpaceLink *sound_new(const bContext *C)
{
	ARegion *ar;
	SpaceSound *ssound;
	
	ssound= MEM_callocN(sizeof(SpaceSound), "initsound");
	ssound->spacetype= SPACE_SOUND;
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for sound");
	
	BLI_addtail(&ssound->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for sound");
	
	BLI_addtail(&ssound->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	ar->v2d.tot.xmin= -4.0f;
	ar->v2d.tot.ymin= -4.0f;
	ar->v2d.tot.xmax= 250.0f;
	ar->v2d.tot.ymax= 255.0f;
	
	ar->v2d.cur.xmin= -4.0f;
	ar->v2d.cur.ymin= -4.0f;
	ar->v2d.cur.xmax= 50.0f;
	ar->v2d.cur.ymax= 255.0f;
	
	ar->v2d.min[0]= 1.0f;
	ar->v2d.min[1]= 259.0f;
	
	ar->v2d.max[0]= MAXFRAMEF;
	ar->v2d.max[1]= 259.0f;
	
	ar->v2d.minzoom= 0.1f;
	ar->v2d.maxzoom= 10.0f;
	
	ar->v2d.scroll = (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
	ar->v2d.scroll |= (V2D_SCROLL_LEFT);
	ar->v2d.keepzoom= 0;
	ar->v2d.keeptot= 0;
	ar->v2d.keepzoom = V2D_LOCKZOOM_Y;
	
	
	return (SpaceLink *)ssound;
}

/* not spacelink itself */
static void sound_free(SpaceLink *sl)
{	
//	SpaceSound *ssound= (SpaceSound*) sl;
	
	
}


/* spacetype; init callback */
static void sound_init(struct wmWindowManager *wm, ScrArea *sa)
{

}

static SpaceLink *sound_duplicate(SpaceLink *sl)
{
	SpaceSound *ssoundn= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	
	return (SpaceLink *)ssoundn;
}



/* add handlers, stuff you only do once or on area/region changes */
static void sound_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_find(wm->defaultconf, "Sound", SPACE_SOUND, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void sound_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	// SpaceSound *ssound= (SpaceSound*)CTX_wm_space_data(C);
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

void sound_operatortypes(void)
{
	
}

void sound_keymap(struct wmKeyConfig *keyconf)
{
	
}

/* add handlers, stuff you only do once or on area/region changes */
static void sound_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);
}

static void sound_header_area_draw(const bContext *C, ARegion *ar)
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
	
	sound_header_buttons(C, ar);
	
	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

static void sound_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_sound(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype sound");
	ARegionType *art;
	
	st->spaceid= SPACE_SOUND;
	strncpy(st->name, "Sound", BKE_ST_MAXNAME);
	
	st->new= sound_new;
	st->free= sound_free;
	st->init= sound_init;
	st->duplicate= sound_duplicate;
	st->operatortypes= sound_operatortypes;
	st->keymap= sound_keymap;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype sound region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= sound_main_area_init;
	art->draw= sound_main_area_draw;
	art->listener= sound_main_area_listener;
	art->keymapflag= ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype sound region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_HEADER;
	
	art->init= sound_header_area_init;
	art->draw= sound_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: channels */
	art= MEM_callocN(sizeof(ARegionType), "spacetype sound region");
	art->regionid = RGN_TYPE_CHANNELS;
	art->minsizex= 80;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
//	art->init= sound_channel_area_init;
//	art->draw= sound_channel_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	
	BKE_spacetype_register(st);
}

