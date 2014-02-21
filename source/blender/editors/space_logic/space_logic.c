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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_logic/space_logic.c
 *  \ingroup splogic
 */


#include <string.h>
#include <stdio.h>


#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
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

#include "logic_intern.h"

/* ******************** manage regions ********************* */

ARegion *logic_has_buttons_region(ScrArea *sa)
{
	ARegion *ar, *arnew;

	ar = BKE_area_find_region_type(sa, RGN_TYPE_UI);
	if (ar) return ar;
	
	/* add subdiv level; after header */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);

	/* is error! */
	if (ar == NULL) return NULL;
	
	arnew= MEM_callocN(sizeof(ARegion), "buttons for image");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype = RGN_TYPE_UI;
	arnew->alignment = RGN_ALIGN_RIGHT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}

/* ******************** default callbacks for image space ***************** */

static SpaceLink *logic_new(const bContext *C)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar;
	SpaceLogic *slogic;
	
	slogic= MEM_callocN(sizeof(SpaceLogic), "initlogic");
	slogic->spacetype= SPACE_LOGIC;
	
	/* default options */
	slogic->scaflag = ((BUTS_SENS_SEL|BUTS_SENS_ACT|BUTS_SENS_LINK) |
	                   (BUTS_CONT_SEL|BUTS_CONT_ACT|BUTS_CONT_LINK) |
	                   (BUTS_ACT_SEL|BUTS_ACT_ACT|BUTS_ACT_LINK)    |
	                   (BUTS_SENS_STATE|BUTS_ACT_STATE));
	
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for logic");
	
	BLI_addtail(&slogic->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* buttons/list view */
	ar= MEM_callocN(sizeof(ARegion), "buttons for logic");
	
	BLI_addtail(&slogic->regionbase, ar);
	ar->regiontype= RGN_TYPE_UI;
	ar->alignment= RGN_ALIGN_RIGHT;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for logic");
	
	BLI_addtail(&slogic->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;

	ar->v2d.tot.xmin =  0.0f;
	ar->v2d.tot.ymax =  0.0f;
	ar->v2d.tot.xmax = 1150.0f;
	ar->v2d.tot.ymin = ( 1150.0f/(float)sa->winx ) * (float)-sa->winy;
	
	ar->v2d.cur = ar->v2d.tot;
	
	ar->v2d.min[0] = 1.0f;
	ar->v2d.min[1] = 1.0f;
	
	ar->v2d.max[0] = 32000.0f;
	ar->v2d.max[1] = 32000.0f;
	
	ar->v2d.minzoom = 0.5f;
	ar->v2d.maxzoom = 1.5f;
	
	ar->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
	ar->v2d.keepzoom = V2D_KEEPZOOM | V2D_LIMITZOOM | V2D_KEEPASPECT;
	ar->v2d.keeptot = V2D_KEEPTOT_BOUNDS;
	ar->v2d.align = V2D_ALIGN_NO_POS_Y | V2D_ALIGN_NO_NEG_X;
	ar->v2d.keepofs = V2D_KEEPOFS_Y;
	
	return (SpaceLink *)slogic;
}

/* not spacelink itself */
static void logic_free(SpaceLink *UNUSED(sl))
{	
//	Spacelogic *slogic= (SpaceLogic *) sl;
	
//	if (slogic->gpd)
// XXX		BKE_gpencil_free(slogic->gpd);
	
}


/* spacetype; init callback */
static void logic_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{

}

static SpaceLink *logic_duplicate(SpaceLink *sl)
{
	SpaceLogic *slogicn= MEM_dupallocN(sl);
		
	return (SpaceLink *)slogicn;
}

static void logic_operatortypes(void)
{
	WM_operatortype_append(LOGIC_OT_properties);
	WM_operatortype_append(LOGIC_OT_links_cut);
}

static void logic_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Logic Editor", SPACE_LOGIC, 0);
	
	WM_keymap_add_item(keymap, "LOGIC_OT_properties", NKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "LOGIC_OT_links_cut", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_menu(keymap, "LOGIC_MT_logicbricks_add", AKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "LOGIC_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "LOGIC_OT_view_all", NDOF_BUTTON_FIT, KM_PRESS, 0, 0);
}

static void logic_refresh(const bContext *UNUSED(C), ScrArea *UNUSED(sa))
{
//	SpaceLogic *slogic= CTX_wm_space_logic(C);
//	Object *obedit= CTX_data_edit_object(C);

}

static void logic_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_LOGIC:
			ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			switch (wmn->data) {
				case ND_FRAME:
					ED_region_tag_redraw(ar);
					break;
					
				case ND_OB_ACTIVE:
					ED_region_tag_redraw(ar);
					break;
				}
				break;
		case NC_OBJECT:
			break;
		case NC_ID:
			if (wmn->action == NA_RENAME)
				ED_region_tag_redraw(ar);
			break;
	}
}

static int logic_context(const bContext *UNUSED(C), const char *UNUSED(member), bContextDataResult *UNUSED(result))
{
//	SpaceLogic *slogic= CTX_wm_space_logic(C);
	return 0;
}

/************************** main region ***************************/


/* add handlers, stuff you only do once or on area/region changes */
static void logic_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	/* own keymaps */
	keymap = WM_keymap_find(wm->defaultconf, "Logic Editor", SPACE_LOGIC, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void logic_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
//	SpaceLogic *slogic= CTX_wm_space_logic(C);
	View2D *v2d= &ar->v2d;
	View2DScrollers *scrollers;
	
	/* clear and setup matrix */
	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(v2d);
	
	logic_buttons((bContext *)C, ar);
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers= UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
	
}


/* *********************** buttons region ************************ */

/* add handlers, stuff you only do once or on area/region changes */
static void logic_buttons_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	ED_region_panels_init(wm, ar);
	
	keymap = WM_keymap_find(wm->defaultconf, "Logic Editor", SPACE_LOGIC, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void logic_buttons_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, NULL, -1);
}

/************************* header region **************************/

/* add handlers, stuff you only do once or on area/region changes */
static void logic_header_area_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void logic_header_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

/**************************** spacetype *****************************/

/* only called once, from space/spacetypes.c */
void ED_spacetype_logic(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype logic");
	ARegionType *art;
	
	st->spaceid = SPACE_LOGIC;
	strncpy(st->name, "Logic", BKE_ST_MAXNAME);
	
	st->new = logic_new;
	st->free = logic_free;
	st->init = logic_init;
	st->duplicate = logic_duplicate;
	st->operatortypes = logic_operatortypes;
	st->keymap = logic_keymap;
	st->refresh = logic_refresh;
	st->context = logic_context;
	
	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype logic region");
	art->regionid = RGN_TYPE_WINDOW;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES | ED_KEYMAP_VIEW2D;
	art->init = logic_main_area_init;
	art->draw = logic_main_area_draw;
	art->listener = logic_listener;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: listview/buttons */
	art = MEM_callocN(sizeof(ARegionType), "spacetype logic region");
	art->regionid = RGN_TYPE_UI;
	art->prefsizex= 220; // XXX
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	art->listener = logic_listener;
	art->init = logic_buttons_area_init;
	art->draw = logic_buttons_area_draw;
	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype logic region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
	art->init = logic_header_area_init;
	art->draw = logic_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	BKE_spacetype_register(st);
}


