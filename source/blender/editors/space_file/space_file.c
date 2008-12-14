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

#include "DNA_image_types.h"
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


/* ******************** default callbacks for image space ***************** */

static SpaceLink *file_new(void)
{
	ARegion *ar;
	SpaceFile *sfile;
	
	sfile= MEM_callocN(sizeof(SpaceImage), "initimage");
	sfile->spacetype= SPACE_FILE;
	
	/* XXX init cleaned up file space */
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for file");
	
	BLI_addtail(&sfile->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	UI_view2d_header_default(&ar->v2d);
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for file");
	
	BLI_addtail(&sfile->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	/* bookmark region XXX */

	/* button region XXX */
	
	return (SpaceLink *)sfile;
}

/* not spacelink itself */
static void file_free(SpaceLink *sl)
{	
	SpaceFile *sfile= (SpaceFile*) sl;
	
	/* XXX free necessary items */
	
}


/* spacetype; init callback */
static void file_init(struct wmWindowManager *wm, ScrArea *sa)
{

}

static SpaceLink *file_duplicate(SpaceLink *sl)
{
	SpaceImage *file_dup= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	
	return (SpaceLink *)file_dup;
}



/* add handlers, stuff you only do once or on area/region changes */
static void file_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	ListBase *keymap;
	
	UI_view2d_size_update(&ar->v2d, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_listbase(wm, "Image", SPACE_IMAGE, 0);	/* XXX weak? */
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void file_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	// SpaceImage *simage= C->area->spacedata.first;
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

void file_operatortypes(void)
{
	
}

void file_keymap(struct wmWindowManager *wm)
{
	
}

/* add handlers, stuff you only do once or on area/region changes */
static void file_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_size_update(&ar->v2d, ar->winx, ar->winy);
}

static void file_header_area_draw(const bContext *C, ARegion *ar)
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
	
	file_header_buttons(C, ar);
	
	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

static void file_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_file(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype file");
	ARegionType *art;
	
	st->spaceid= SPACE_FILE;
	
	st->new= file_new;
	st->free= file_free;
	st->init= file_init;
	st->duplicate= file_duplicate;
	st->operatortypes= file_operatortypes;
	st->keymap= file_keymap;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype file region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= file_main_area_init;
	art->draw= file_main_area_draw;
	art->listener= file_main_area_listener;
	art->keymapflag= ED_KEYMAP_VIEW2D;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype file region");
	art->regionid = RGN_TYPE_HEADER;
	art->init= file_header_area_init;
	art->draw= file_header_area_draw;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;	
	
	BLI_addhead(&st->regiontypes, art);

	BKE_spacetype_register(st);
}

