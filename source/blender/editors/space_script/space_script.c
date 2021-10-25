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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_script/space_script.c
 *  \ingroup spscript
 */


#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#ifdef WITH_PYTHON
#endif

#include "script_intern.h"  // own include


//static script_run_python(char *funcname, )


/* ******************** default callbacks for script space ***************** */

static SpaceLink *script_new(const bContext *UNUSED(C))
{
	ARegion *ar;
	SpaceScript *sscript;
	
	sscript = MEM_callocN(sizeof(SpaceScript), "initscript");
	sscript->spacetype = SPACE_SCRIPT;
	
	
	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for script");
	
	BLI_addtail(&sscript->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;
	
	/* main region */
	ar = MEM_callocN(sizeof(ARegion), "main region for script");
	
	BLI_addtail(&sscript->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;
	
	/* channel list region XXX */

	
	return (SpaceLink *)sscript;
}

/* not spacelink itself */
static void script_free(SpaceLink *sl)
{	
	SpaceScript *sscript = (SpaceScript *) sl;

#ifdef WITH_PYTHON
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
static void script_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{

}

static SpaceLink *script_duplicate(SpaceLink *sl)
{
	SpaceScript *sscriptn = MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	
	return (SpaceLink *)sscriptn;
}



/* add handlers, stuff you only do once or on area/region changes */
static void script_main_region_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_STANDARD, ar->winx, ar->winy);
	
	/* own keymap */
	keymap = WM_keymap_find(wm->defaultconf, "Script", SPACE_SCRIPT, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void script_main_region_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceScript *sscript = (SpaceScript *)CTX_wm_space_data(C);
	View2D *v2d = &ar->v2d;

	/* clear and setup matrix */
	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(v2d);
		
	/* data... */
	// BPY_script_exec(C, "/root/blender-svn/blender25/test.py", NULL);
	
#ifdef WITH_PYTHON
	if (sscript->script) {
		// BPY_run_script_space_draw(C, sscript);
	}
#else
	(void)sscript;
#endif
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers? */
}

/* add handlers, stuff you only do once or on area/region changes */
static void script_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void script_header_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void script_main_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *UNUSED(ar), wmNotifier *UNUSED(wmn))
{
	/* context changes */
	// XXX - Todo, need the ScriptSpace accessible to get the python script to run.
	// BPY_run_script_space_listener()
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_script(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype script");
	ARegionType *art;
	
	st->spaceid = SPACE_SCRIPT;
	strncpy(st->name, "Script", BKE_ST_MAXNAME);
	
	st->new = script_new;
	st->free = script_free;
	st->init = script_init;
	st->duplicate = script_duplicate;
	st->operatortypes = script_operatortypes;
	st->keymap = script_keymap;
	
	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype script region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init = script_main_region_init;
	art->draw = script_main_region_draw;
	art->listener = script_main_region_listener;
	art->keymapflag = ED_KEYMAP_VIEW2D |   ED_KEYMAP_UI | ED_KEYMAP_FRAMES; // XXX need to further test this ED_KEYMAP_UI is needed for button interaction

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype script region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
	
	art->init = script_header_region_init;
	art->draw = script_header_region_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	
	BKE_spacetype_register(st);
}

