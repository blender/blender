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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_collections/space_collections.c
 *  \ingroup spcollections
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BIF_gl.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "collections_intern.h" /* own include */

/* ******************** default callbacks for collection manager space ***************** */

static SpaceLink *collections_new(const bContext *UNUSED(C))
{
	ARegion *ar;
	SpaceCollections *scollection; /* hmm, that's actually a good band name... */

	scollection = MEM_callocN(sizeof(SpaceCollections), __func__);
	scollection->spacetype = SPACE_COLLECTIONS;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for collection manager");
	BLI_addtail(&scollection->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;

	/* main region */
	ar = MEM_callocN(sizeof(ARegion), "main region for collection manager");
	BLI_addtail(&scollection->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;
	ar->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HIDE | V2D_SCROLL_VERTICAL_HIDE);
	ar->v2d.align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y);

	return (SpaceLink *)scollection;
}

static void collections_free(SpaceLink *UNUSED(sl))
{
}

static SpaceLink *collections_duplicate(SpaceLink *sl)
{
	SpaceCollections *scollection = MEM_dupallocN(sl);

	/* clear or remove stuff from old */

	return (SpaceLink *)scollection;
}

/* add handlers, stuff you only do once or on area/region changes */
static void collection_main_region_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);
	ar->v2d.scroll |= (V2D_SCROLL_VERTICAL_FULLR | V2D_SCROLL_HORIZONTAL_FULLR);

	/* own keymap */
	wmKeyMap *keymap = WM_keymap_find(wm->defaultconf, "Layer Manager", SPACE_COLLECTIONS, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void collections_main_region_draw(const bContext *C, ARegion *ar)
{
	SpaceCollections *spc = CTX_wm_space_collections(C);
	View2D *v2d = &ar->v2d;

	if (spc->flag & SC_COLLECTION_DATA_REFRESH) {
	}

	/* v2d has initialized flag, so this call will only set the mask correct */
	UI_view2d_region_reinit(v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);

	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);

	/* reset view matrix */
	UI_view2d_view_restore(C);

	/* scrollers */
	View2DScrollers *scrollers;
	scrollers = UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

/* add handlers, stuff you only do once or on area/region changes */
static void collections_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void collections_header_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void collections_main_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	switch (wmn->category) {
		case NC_SCENE:
			if (wmn->data == ND_LAYER) {
				ED_region_tag_redraw(ar);
			}
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_COLLECTIONS) {
				ED_region_tag_redraw(ar);
			}
	}
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_collections(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype collections");
	ARegionType *art;

	st->spaceid = SPACE_COLLECTIONS;
	strncpy(st->name, "LayerManager", BKE_ST_MAXNAME);

	st->new = collections_new;
	st->free = collections_free;
	st->duplicate = collections_duplicate;
	st->operatortypes = collections_operatortypes;
	st->keymap = collections_keymap;

	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype collections region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init = collection_main_region_init;
	art->draw = collections_main_region_draw;
	art->listener = collections_main_region_listener;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;
	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype collections header");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
	art->init = collections_header_region_init;
	art->draw = collections_header_region_draw;
	BLI_addhead(&st->regiontypes, art);

	BKE_spacetype_register(st);
}
