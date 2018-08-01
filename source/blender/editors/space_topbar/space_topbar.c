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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_topbar/space_topbar.c
 *  \ingroup sptopbar
 */


#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLO_readfile.h"
#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_undo.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

/* ******************** default callbacks for topbar space ***************** */

static SpaceLink *topbar_new(const ScrArea *UNUSED(area), const Scene *UNUSED(scene))
{
	ARegion *ar;
	SpaceTopBar *stopbar;

	stopbar = MEM_callocN(sizeof(*stopbar), "init topbar");
	stopbar->spacetype = SPACE_TOPBAR;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "left aligned header for topbar");
	BLI_addtail(&stopbar->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_TOP;
	ar = MEM_callocN(sizeof(ARegion), "right aligned header for topbar");
	BLI_addtail(&stopbar->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_RIGHT | RGN_SPLIT_PREV;

	/* main regions */
	ar = MEM_callocN(sizeof(ARegion), "left aligned main region for topbar");
	BLI_addtail(&stopbar->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;
	ar->alignment = RGN_ALIGN_LEFT;
	ar = MEM_callocN(sizeof(ARegion), "right aligned main region for topbar");
	BLI_addtail(&stopbar->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;
	ar->alignment = RGN_ALIGN_RIGHT;
	ar = MEM_callocN(sizeof(ARegion), "center main region for topbar");
	BLI_addtail(&stopbar->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;

	return (SpaceLink *)stopbar;
}

/* not spacelink itself */
static void topbar_free(SpaceLink *UNUSED(sl))
{

}


/* spacetype; init callback */
static void topbar_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{

}

static SpaceLink *topbar_duplicate(SpaceLink *sl)
{
	SpaceTopBar *stopbarn = MEM_dupallocN(sl);

	/* clear or remove stuff from old */

	return (SpaceLink *)stopbarn;
}



/* add handlers, stuff you only do once or on area/region changes */
static void topbar_main_region_init(wmWindowManager *wm, ARegion *region)
{
	wmKeyMap *keymap;

	/* force delayed UI_view2d_region_reinit call */
	if (ELEM(region->alignment, RGN_ALIGN_LEFT, RGN_ALIGN_RIGHT)) {
		region->flag |= RGN_FLAG_DYNAMIC_SIZE;
	}
	UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_HEADER, region->winx, region->winy);

	keymap = WM_keymap_find(wm->defaultconf, "View2D Buttons List", 0, 0);
	WM_event_add_keymap_handler(&region->handlers, keymap);
}

static void topbar_operatortypes(void)
{

}

static void topbar_keymap(struct wmKeyConfig *UNUSED(keyconf))
{

}

/* add handlers, stuff you only do once or on area/region changes */
static void topbar_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	if ((ar->alignment & ~RGN_SPLIT_PREV) == RGN_ALIGN_RIGHT) {
		ar->flag |= RGN_FLAG_DYNAMIC_SIZE;
	}
	ED_region_header_init(ar);
}

static void topbar_main_region_listener(wmWindow *UNUSED(win), ScrArea *UNUSED(sa), ARegion *ar,
                                        wmNotifier *wmn, const Scene *UNUSED(scene))
{
	/* context changes */
	switch (wmn->category) {
		case NC_WM:
			if (wmn->data == ND_HISTORY)
				ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			if (wmn->data == ND_MODE)
				ED_region_tag_redraw(ar);
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_VIEW3D)
				ED_region_tag_redraw(ar);
			break;
		case NC_GPENCIL:
			if (wmn->data == ND_DATA)
				ED_region_tag_redraw(ar);
			break;
	}
}

static void topbar_header_listener(wmWindow *UNUSED(win), ScrArea *UNUSED(sa), ARegion *ar,
                                   wmNotifier *wmn, const Scene *UNUSED(scene))
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCREEN:
			if (wmn->data == ND_LAYER)
				ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			if (wmn->data == ND_SCENEBROWSE)
				ED_region_tag_redraw(ar);
			break;
	}
}

static void topbar_header_region_message_subscribe(
        const struct bContext *UNUSED(C),
        struct WorkSpace *workspace, struct Scene *UNUSED(scene),
        struct bScreen *UNUSED(screen), struct ScrArea *UNUSED(sa), struct ARegion *ar,
        struct wmMsgBus *mbus)
{
	wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
		.owner = ar,
		.user_data = ar,
		.notify = ED_region_do_msg_notify_tag_redraw,
	};

	WM_msg_subscribe_rna_prop(
	        mbus, &workspace->id, workspace,
	        WorkSpace, tools, &msg_sub_value_region_tag_redraw);
}

static void recent_files_menu_draw(const bContext *UNUSED(C), Menu *menu)
{
	struct RecentFile *recent;
	uiLayout *layout = menu->layout;
	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
	if (!BLI_listbase_is_empty(&G.recent_files)) {
		for (recent = G.recent_files.first; (recent); recent = recent->next) {
			const char *file = BLI_path_basename(recent->filepath);
			const int icon = BLO_has_bfile_extension(file) ? ICON_FILE_BLEND : ICON_FILE_BACKUP;
			uiItemStringO(layout, file, icon, "WM_OT_open_mainfile", "filepath", recent->filepath);
		}
	}
	else {
		uiItemL(layout, IFACE_("No Recent Files"), ICON_NONE);
	}
}

static void recent_files_menu_register(void)
{
	MenuType *mt;

	mt = MEM_callocN(sizeof(MenuType), "spacetype info menu recent files");
	strcpy(mt->idname, "TOPBAR_MT_file_open_recent");
	strcpy(mt->label, N_("Open Recent..."));
	strcpy(mt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
	mt->draw = recent_files_menu_draw;
	WM_menutype_add(mt);
}


/* only called once, from space/spacetypes.c */
void ED_spacetype_topbar(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype topbar");
	ARegionType *art;

	st->spaceid = SPACE_TOPBAR;
	strncpy(st->name, "Top Bar", BKE_ST_MAXNAME);

	st->new = topbar_new;
	st->free = topbar_free;
	st->init = topbar_init;
	st->duplicate = topbar_duplicate;
	st->operatortypes = topbar_operatortypes;
	st->keymap = topbar_keymap;

	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype topbar main region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init = topbar_main_region_init;
	art->layout = ED_region_header_layout;
	art->draw = ED_region_header_draw;
	art->listener = topbar_main_region_listener;
	art->prefsizex = UI_UNIT_X * 5; /* Mainly to avoid glitches */
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype topbar header region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->prefsizex = UI_UNIT_X * 5; /* Mainly to avoid glitches */
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
	art->listener = topbar_header_listener;
	art->message_subscribe = topbar_header_region_message_subscribe;
	art->init = topbar_header_region_init;
	art->layout = ED_region_header_layout;
	art->draw = ED_region_header_draw;

	BLI_addhead(&st->regiontypes, art);

	recent_files_menu_register();

	BKE_spacetype_register(st);
}
