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

/** \file blender/editors/space_outliner/space_outliner.c
 *  \ingroup spoutliner
 */


#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"

#include "RNA_access.h"

#include "UI_resources.h"
#include "UI_view2d.h"


#include "outliner_intern.h"

static void outliner_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	ListBase *lb;
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);
	
	/* own keymap */
	keymap = WM_keymap_find(wm->defaultconf, "Outliner", SPACE_OUTLINER, 0);
	/* don't pass on view2d mask, it's always set with scrollbar space, hide fails */
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, NULL, &ar->winrct);

	/* Add dropboxes */
	lb = WM_dropboxmap_find("Outliner", SPACE_OUTLINER, RGN_TYPE_WINDOW);
	WM_event_add_dropbox_handler(&ar->handlers, lb);
}

static int outliner_parent_drop_poll(bContext *C, wmDrag *drag, wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te = NULL;
	float fmval[2];
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	if (drag->type == WM_DRAG_ID) {
		ID *id = (ID *)drag->poin;
		if (GS(id->name) == ID_OB) {
			/* Ensure item under cursor is valid drop target */
			/* Find object hovered over */
			for (te = soops->tree.first; te; te = te->next) {
				TreeElement *te_valid;
				te_valid = outliner_dropzone_parent(C, event, te, fmval);
				if (te_valid) return 1;
			}
		}
	}
	return 0;
}

static void outliner_parent_drop_copy(wmDrag *drag, wmDropBox *drop)
{
	ID *id = (ID *)drag->poin;

	RNA_string_set(drop->ptr, "child", id->name + 2);
}

static int outliner_parent_clear_poll(bContext *C, wmDrag *drag, wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te = NULL;
	float fmval[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	if (drag->type == WM_DRAG_ID) {
		ID *id = (ID *)drag->poin;
		if (GS(id->name) == ID_OB) {
			//TODO: Check if no parent?
			/* Ensure location under cursor is valid dropzone */
			for (te = soops->tree.first; te; te = te->next) {
				if (outliner_dropzone_parent_clear(C, event, te, fmval)) return 1;
			}
			/* Check if mouse cursor is below the tree */
			te = soops->tree.last;
			while (((te->flag & TE_LAZY_CLOSED) == 0) && (te->subtree.last)) {
				te = te->subtree.last;
			}
			if (fmval[1] < te->ys) return 1;
		}
	}
	return 0;
}

static void outliner_parent_clear_copy(wmDrag *drag, wmDropBox *drop)
{
	ID *id = (ID *)drag->poin;
	RNA_string_set(drop->ptr, "dragged_obj", id->name + 2);

	/* Set to simple parent clear type. Avoid menus for drag and drop if possible.
	 * If desired, user can toggle the different "Clear Parent" types in the operator
	 * menu on tool shelf. */
	RNA_enum_set(drop->ptr, "type", 0);
}

/* region dropbox definition */
static void outliner_dropboxes(void)
{
	ListBase *lb = WM_dropboxmap_find("Outliner", SPACE_OUTLINER, RGN_TYPE_WINDOW);

	WM_dropbox_add(lb, "OUTLINER_OT_parent_drop", outliner_parent_drop_poll, outliner_parent_drop_copy);
	WM_dropbox_add(lb, "OUTLINER_OT_parent_clear", outliner_parent_clear_poll, outliner_parent_clear_copy);
}

static void outliner_main_area_draw(const bContext *C, ARegion *ar)
{
	View2D *v2d = &ar->v2d;
	View2DScrollers *scrollers;
	
	/* clear */
	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);
	
	draw_outliner(C);
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers = UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}


static void outliner_main_area_free(ARegion *UNUSED(ar))
{
	
}

static void outliner_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCENE:
			switch (wmn->data) {
				case ND_OB_ACTIVE:
				case ND_OB_SELECT:
				case ND_OB_VISIBLE:
				case ND_OB_RENDER:
				case ND_MODE:
				case ND_KEYINGSET:
				case ND_FRAME:
				case ND_RENDER_OPTIONS:
				case ND_LAYER:
				case ND_WORLD:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_OBJECT:
			switch (wmn->data) {
				case ND_TRANSFORM:
					/* transform doesn't change outliner data */
					break;
				case ND_BONE_ACTIVE:
				case ND_BONE_SELECT:
				case ND_DRAW:
				case ND_PARENT:
				case ND_OB_SHADING:
					ED_region_tag_redraw(ar);
					break;
				case ND_CONSTRAINT:
					switch (wmn->action) {
						case NA_ADDED:
						case NA_REMOVED:
						case NA_RENAME:
							ED_region_tag_redraw(ar);
							break;
					}
					break;
				case ND_MODIFIER:
					/* all modifier actions now */
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_GROUP:
			/* all actions now, todo: check outliner view mode? */
			ED_region_tag_redraw(ar);
			break;
		case NC_LAMP:
			/* For updating lamp icons, when changing lamp type */
			if (wmn->data == ND_LIGHTING_DRAW)
				ED_region_tag_redraw(ar);
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_OUTLINER)
				ED_region_tag_redraw(ar);
			break;
		case NC_ID:
			if (wmn->action == NA_RENAME)
				ED_region_tag_redraw(ar);
			break;
		case NC_MATERIAL:
			switch (wmn->data) {
				case ND_SHADING:
				case ND_SHADING_DRAW:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_TEXTURE:
			ED_region_tag_redraw(ar);
			break;
		case NC_GEOM:
			switch (wmn->data) {
				case ND_DATA:
					/* needed for vertex groups only, no special notifier atm so use NC_GEOM|ND_DATA */
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_ANIMATION:
			switch (wmn->data) {
				case ND_NLA_ACTCHANGE:
				case ND_KEYFRAME:
					ED_region_tag_redraw(ar);
					break;
				case ND_ANIMCHAN:
					if (wmn->action == NA_SELECTED)
						ED_region_tag_redraw(ar);
					break;
			}
			break;
	}
	
}


/* ************************ header outliner area region *********************** */

/* add handlers, stuff you only do once or on area/region changes */
static void outliner_header_area_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void outliner_header_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void outliner_header_area_free(ARegion *UNUSED(ar))
{
}

static void outliner_header_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCENE:
			if (wmn->data == ND_KEYINGSET)
				ED_region_tag_redraw(ar);
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_OUTLINER)
				ED_region_tag_redraw(ar);
			break;
	}
}

/* ******************** default callbacks for outliner space ***************** */

static SpaceLink *outliner_new(const bContext *UNUSED(C))
{
	ARegion *ar;
	SpaceOops *soutliner;

	soutliner = MEM_callocN(sizeof(SpaceOops), "initoutliner");
	soutliner->spacetype = SPACE_OUTLINER;
	
	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for outliner");
	
	BLI_addtail(&soutliner->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;
	
	/* main area */
	ar = MEM_callocN(sizeof(ARegion), "main area for outliner");
	
	BLI_addtail(&soutliner->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;
	
	ar->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM_O);
	ar->v2d.align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y);
	ar->v2d.keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
	ar->v2d.keeptot = V2D_KEEPTOT_STRICT;
	ar->v2d.minzoom = ar->v2d.maxzoom = 1.0f;
	
	return (SpaceLink *)soutliner;
}

/* not spacelink itself */
static void outliner_free(SpaceLink *sl)
{
	SpaceOops *soutliner = (SpaceOops *)sl;
	
	outliner_free_tree(&soutliner->tree);
	if (soutliner->treestore) {
		if (soutliner->treestore->data) MEM_freeN(soutliner->treestore->data);
		MEM_freeN(soutliner->treestore);
	}
	
}

/* spacetype; init callback */
static void outliner_init(wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{
	
}

static SpaceLink *outliner_duplicate(SpaceLink *sl)
{
	SpaceOops *soutliner = (SpaceOops *)sl;
	SpaceOops *soutlinern = MEM_dupallocN(soutliner);

	soutlinern->tree.first = soutlinern->tree.last = NULL;
	soutlinern->treestore = NULL;
	
	return (SpaceLink *)soutlinern;
}

/* only called once, from space_api/spacetypes.c */
void ED_spacetype_outliner(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype time");
	ARegionType *art;
	
	st->spaceid = SPACE_OUTLINER;
	strncpy(st->name, "Outliner", BKE_ST_MAXNAME);
	
	st->new = outliner_new;
	st->free = outliner_free;
	st->init = outliner_init;
	st->duplicate = outliner_duplicate;
	st->operatortypes = outliner_operatortypes;
	st->keymap = outliner_keymap;
	st->dropboxes = outliner_dropboxes;
	
	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype time region");
	art->regionid = RGN_TYPE_WINDOW;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;
	
	art->init = outliner_main_area_init;
	art->draw = outliner_main_area_draw;
	art->free = outliner_main_area_free;
	art->listener = outliner_main_area_listener;
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype time region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
	
	art->init = outliner_header_area_init;
	art->draw = outliner_header_area_draw;
	art->free = outliner_header_area_free;
	art->listener = outliner_header_area_listener;
	BLI_addhead(&st->regiontypes, art);
	
	BKE_spacetype_register(st);
}

