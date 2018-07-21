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

/** \file blender/editors/space_statusbar/space_statusbar.c
 *  \ingroup spstatusbar
 */


#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"


/* ******************** default callbacks for statusbar space ********************  */

static SpaceLink *statusbar_new(const ScrArea *UNUSED(area), const Scene *UNUSED(scene))
{
	ARegion *ar;
	SpaceStatusBar *sstatusbar;

	sstatusbar = MEM_callocN(sizeof(*sstatusbar), "init statusbar");
	sstatusbar->spacetype = SPACE_STATUSBAR;

	/* header region */
	ar = MEM_callocN(sizeof(*ar), "header for statusbar");
	BLI_addtail(&sstatusbar->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_NONE;

	return (SpaceLink *)sstatusbar;
}

/* not spacelink itself */
static void statusbar_free(SpaceLink *UNUSED(sl))
{

}


/* spacetype; init callback */
static void statusbar_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{

}

static SpaceLink *statusbar_duplicate(SpaceLink *sl)
{
	SpaceStatusBar *sstatusbarn = MEM_dupallocN(sl);

	/* clear or remove stuff from old */

	return (SpaceLink *)sstatusbarn;
}



/* add handlers, stuff you only do once or on area/region changes */
static void statusbar_header_region_init(wmWindowManager *UNUSED(wm), ARegion *region)
{
	if (ELEM(region->alignment, RGN_ALIGN_RIGHT)) {
		region->flag |= RGN_FLAG_DYNAMIC_SIZE;
	}
	ED_region_header_init(region);
}

static void statusbar_operatortypes(void)
{

}

static void statusbar_keymap(struct wmKeyConfig *UNUSED(keyconf))
{

}

static void statusbar_header_region_listener(
        wmWindow *UNUSED(win), ScrArea *UNUSED(sa), ARegion *ar,
        wmNotifier *wmn, const Scene *UNUSED(scene))
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCREEN:
			if (ELEM(wmn->data, ND_LAYER, ND_ANIMPLAY)) {
				ED_region_tag_redraw(ar);
			}
			break;
		case NC_WM:
			if (wmn->data == ND_JOB)
				ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			if (wmn->data == ND_RENDER_RESULT)
				ED_region_tag_redraw(ar);
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_INFO)
				ED_region_tag_redraw(ar);
			break;
		case NC_ID:
			if (wmn->action == NA_RENAME)
				ED_region_tag_redraw(ar);
			break;
	}
}

static void statusbar_header_region_message_subscribe(
        const bContext *UNUSED(C),
        WorkSpace *UNUSED(workspace), Scene *UNUSED(scene),
        bScreen *UNUSED(screen), ScrArea *UNUSED(sa), ARegion *ar,
        struct wmMsgBus *mbus)
{
	wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
		.owner = ar,
		.user_data = ar,
		.notify = ED_region_do_msg_notify_tag_redraw,
	};

	WM_msg_subscribe_rna_anon_prop(mbus, Window, view_layer, &msg_sub_value_region_tag_redraw);
	WM_msg_subscribe_rna_anon_prop(mbus, ViewLayer, name, &msg_sub_value_region_tag_redraw);
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_statusbar(void)
{
	SpaceType *st = MEM_callocN(sizeof(*st), "spacetype statusbar");
	ARegionType *art;

	st->spaceid = SPACE_STATUSBAR;
	strncpy(st->name, "Status Bar", BKE_ST_MAXNAME);

	st->new = statusbar_new;
	st->free = statusbar_free;
	st->init = statusbar_init;
	st->duplicate = statusbar_duplicate;
	st->operatortypes = statusbar_operatortypes;
	st->keymap = statusbar_keymap;

	/* regions: header window */
	art = MEM_callocN(sizeof(*art), "spacetype statusbar header region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = 0.8f * HEADERY;
	art->prefsizex = UI_UNIT_X * 5; /* Mainly to avoid glitches */
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
	art->init = statusbar_header_region_init;
	art->layout = ED_region_header_layout;
	art->draw = ED_region_header_draw;
	art->listener = statusbar_header_region_listener;
	art->message_subscribe = statusbar_header_region_message_subscribe;
	BLI_addhead(&st->regiontypes, art);

	BKE_spacetype_register(st);
}
