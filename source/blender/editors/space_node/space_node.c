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

#include "DNA_node_types.h"
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

#include "node_intern.h"	// own include

/* ******************** default callbacks for node space ***************** */

static SpaceLink *node_new(void)
{
	ARegion *ar;
	SpaceNode *snode;
	
	snode= MEM_callocN(sizeof(SpaceNode), "initnode");
	snode->spacetype= SPACE_NODE;	
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for node");
	
	BLI_addtail(&snode->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for node");
	
	BLI_addtail(&snode->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	ar->v2d.tot.xmin=  -10.0f;
	ar->v2d.tot.ymin=  -10.0f;
	ar->v2d.tot.xmax= 512.0f;
	ar->v2d.tot.ymax= 512.0f;
	
	ar->v2d.cur.xmin=  0.0f;
	ar->v2d.cur.ymin=  0.0f;
	ar->v2d.cur.xmax= 512.0f;
	ar->v2d.cur.ymax= 512.0f;
	
	ar->v2d.min[0]= 1.0f;
	ar->v2d.min[1]= 1.0f;
	
	ar->v2d.max[0]= 32000.0f;
	ar->v2d.max[1]= 32000.0f;
	
	ar->v2d.minzoom= 0.5f;
	ar->v2d.maxzoom= 1.21f;
	
	ar->v2d.scroll= 0;
	ar->v2d.keepzoom= V2D_KEEPZOOM|V2D_KEEPASPECT;
	ar->v2d.keeptot= 0;
	
	
	return (SpaceLink *)snode;
}

/* not spacelink itself */
static void node_free(SpaceLink *sl)
{	
//	SpaceNode *snode= (SpaceNode*) sl;
	
// XXX	if(snode->gpd) free_gpencil_data(snode->gpd);
}


/* spacetype; init callback */
static void node_init(struct wmWindowManager *wm, ScrArea *sa)
{

}

static SpaceLink *node_duplicate(SpaceLink *sl)
{
	SpaceNode *snoden= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	snoden->nodetree= NULL;
// XXX	snoden->gpd= gpencil_data_duplicate(snode->gpd);
	
	return (SpaceLink *)snoden;
}



/* add handlers, stuff you only do once or on area/region changes */
static void node_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	ListBase *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_listbase(wm, "Node", SPACE_NODE, 0);	/* XXX weak? */
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void node_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	// SpaceNode *snode= C->area->spacedata.first;
	View2D *v2d= &ar->v2d;
	//View2DGrid *grid;
	float col[3];
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(C, v2d);
	
#if 0
	/* grid */
	grid= UI_view2d_grid_calc(C, v2d, V2D_UNIT_VALUES, V2D_GRID_CLAMP, V2D_UNIT_VALUES, V2D_GRID_CLAMP, ar->winx, ar->winy);
	UI_view2d_grid_draw(C, v2d, grid, V2D_GRIDLINES_ALL);
	UI_view2d_grid_free(grid);
#endif
	
	/* data... */
	
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers? */
}

void node_operatortypes(void)
{
	
}

void node_keymap(struct wmWindowManager *wm)
{
	
}

/* add handlers, stuff you only do once or on area/region changes */
static void node_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);
}

static void node_header_area_draw(const bContext *C, ARegion *ar)
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
	
	node_header_buttons(C, ar);
	
	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

static void node_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_node(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype node");
	ARegionType *art;
	
	st->spaceid= SPACE_NODE;
	
	st->new= node_new;
	st->free= node_free;
	st->init= node_init;
	st->duplicate= node_duplicate;
	st->operatortypes= node_operatortypes;
	st->keymap= node_keymap;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype node region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= node_main_area_init;
	art->draw= node_main_area_draw;
	art->listener= node_main_area_listener;
	art->keymapflag= ED_KEYMAP_VIEW2D;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype node region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
	art->init= node_header_area_init;
	art->draw= node_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: channels */
	art= MEM_callocN(sizeof(ARegionType), "spacetype node region");
	art->regionid = RGN_TYPE_CHANNELS;
	art->minsizex= 80;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
//	art->init= node_channel_area_init;
//	art->draw= node_channel_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	
	BKE_spacetype_register(st);
}

