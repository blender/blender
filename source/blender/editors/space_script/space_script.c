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

#include "BPY_extern.h"

#include "script_intern.h"	// own include


//static script_run_python(char *funcname, )


/* ******************** default callbacks for script space ***************** */

static SpaceLink *script_new(const bContext *C)
{
	ARegion *ar;
	SpaceScript *sscript;
	
	sscript= MEM_callocN(sizeof(SpaceScript), "initscript");
	sscript->spacetype= SPACE_SCRIPT;
	
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for script");
	
	BLI_addtail(&sscript->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for script");
	
	BLI_addtail(&sscript->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	/* channel list region XXX */

	
	return (SpaceLink *)sscript;
}

/* not spacelink itself */
static void script_free(SpaceLink *sl)
{	
	SpaceScript *sscript= (SpaceScript*) sl;

#ifndef DISABLE_PYTHON	
	/*free buttons references*/
	if (sscript->but_refs) {
// XXX		BPy_Set_DrawButtonsList(sscript->but_refs);
//		BPy_Free_DrawButtonsList();
		sscript->but_refs = NULL;
	}
#endif
	sscript->script = NULL;
}


/* spacetype; init callback */
static void script_init(struct wmWindowManager *wm, ScrArea *sa)
{

}

static SpaceLink *script_duplicate(SpaceLink *sl)
{
	SpaceScript *sscriptn= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	
	return (SpaceLink *)sscriptn;
}



/* add handlers, stuff you only do once or on area/region changes */
static void script_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_STANDARD, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_find(wm->defaultconf, "Script", SPACE_SCRIPT, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void script_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceScript *sscript= (SpaceScript*)CTX_wm_space_data(C);
	View2D *v2d= &ar->v2d;
	float col[3];
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(C, v2d);
		
	/* data... */
	// BPY_run_python_script(C, "/root/blender-svn/blender25/test.py", NULL);
	
#ifndef DISABLE_PYTHON
	if (sscript->script) {
		//BPY_run_python_script_space(scpt->script.filename, NULL);
		BPY_run_script_space_draw(C, sscript);
	}
#endif
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers? */
}

/* add handlers, stuff you only do once or on area/region changes */
static void script_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	ED_region_header_init(ar);
}

static void script_header_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void script_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	// XXX - Todo, need the ScriptSpace accessible to get the python script to run.
	// BPY_run_script_space_listener()
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_script(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype script");
	ARegionType *art;
	
	st->spaceid= SPACE_SCRIPT;
	
	st->new= script_new;
	st->free= script_free;
	st->init= script_init;
	st->duplicate= script_duplicate;
	st->operatortypes= script_operatortypes;
	st->keymap= script_keymap;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype script region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= script_main_area_init;
	art->draw= script_main_area_draw;
	art->listener= script_main_area_listener;
	art->keymapflag= ED_KEYMAP_VIEW2D|   ED_KEYMAP_UI|ED_KEYMAP_FRAMES; // XXX need to further test this ED_KEYMAP_UI is needed for button interaction

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype script region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
	art->init= script_header_area_init;
	art->draw= script_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	
	BKE_spacetype_register(st);
}

