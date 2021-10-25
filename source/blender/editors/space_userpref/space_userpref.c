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

/** \file blender/editors/space_userpref/space_userpref.c
 *  \ingroup spuserpref
 */


#include <string.h>
#include <stdio.h>


#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "WM_api.h"
#include "WM_types.h"



/* ******************** default callbacks for userpref space ***************** */

static SpaceLink *userpref_new(const bContext *UNUSED(C))
{
	ARegion *ar;
	SpaceUserPref *spref;

	spref = MEM_callocN(sizeof(SpaceUserPref), "inituserpref");
	spref->spacetype = SPACE_USERPREF;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for userpref");

	BLI_addtail(&spref->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;

	/* main region */
	ar = MEM_callocN(sizeof(ARegion), "main region for userpref");

	BLI_addtail(&spref->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;

	return (SpaceLink *)spref;
}

/* not spacelink itself */
static void userpref_free(SpaceLink *UNUSED(sl))
{	
//	SpaceUserPref *spref = (SpaceUserPref *)sl;
	
}


/* spacetype; init callback */
static void userpref_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{

}

static SpaceLink *userpref_duplicate(SpaceLink *sl)
{
	SpaceUserPref *sprefn = MEM_dupallocN(sl);

	/* clear or remove stuff from old */

	return (SpaceLink *)sprefn;
}



/* add handlers, stuff you only do once or on area/region changes */
static void userpref_main_region_init(wmWindowManager *wm, ARegion *ar)
{
	/* do not use here, the properties changed in userprefs do a system-wide refresh, then scroller jumps back */
	/*	ar->v2d.flag &= ~V2D_IS_INITIALISED; */
	
	ar->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;

	ED_region_panels_init(wm, ar);
}

static void userpref_main_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, NULL, -1, true);
}

static void userpref_operatortypes(void)
{
}

static void userpref_keymap(struct wmKeyConfig *UNUSED(keyconf))
{
	
}

/* add handlers, stuff you only do once or on area/region changes */
static void userpref_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void userpref_header_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void userpref_main_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *UNUSED(ar), wmNotifier *UNUSED(wmn))
{
	/* context changes */
}

static void userpref_header_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *UNUSED(ar), wmNotifier *UNUSED(wmn))
{
	/* context changes */
#if 0
	switch (wmn->category) {
		default:
			break;
	}
#endif
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_userpref(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype userpref");
	ARegionType *art;

	st->spaceid = SPACE_USERPREF;
	strncpy(st->name, "Userpref", BKE_ST_MAXNAME);

	st->new = userpref_new;
	st->free = userpref_free;
	st->init = userpref_init;
	st->duplicate = userpref_duplicate;
	st->operatortypes = userpref_operatortypes;
	st->keymap = userpref_keymap;

	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype userpref region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init = userpref_main_region_init;
	art->draw = userpref_main_region_draw;
	art->listener = userpref_main_region_listener;
	art->keymapflag = ED_KEYMAP_UI;

	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype userpref region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
	art->listener = userpref_header_listener;
	art->init = userpref_header_region_init;
	art->draw = userpref_header_region_draw;

	BLI_addhead(&st->regiontypes, art);


	BKE_spacetype_register(st);
}

