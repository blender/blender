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

/** \file blender/editors/space_action/space_action.c
 *  \ingroup spaction
 */


#include <string.h>
#include <stdio.h>

#include "DNA_action_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_anim_api.h"
#include "ED_markers.h"

#include "action_intern.h"  /* own include */

/* ******************** default callbacks for action space ***************** */

static SpaceLink *action_new(const bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	SpaceAction *saction;
	ARegion *ar;
	
	saction = MEM_callocN(sizeof(SpaceAction), "initaction");
	saction->spacetype = SPACE_ACTION;
	
	saction->autosnap = SACTSNAP_FRAME;
	saction->mode = SACTCONT_DOPESHEET;
	
	saction->ads.filterflag |= ADS_FILTER_SUMMARY;
	
	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for action");
	
	BLI_addtail(&saction->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;
	
	/* channel list region */
	ar = MEM_callocN(sizeof(ARegion), "channel area for action");
	BLI_addtail(&saction->regionbase, ar);
	ar->regiontype = RGN_TYPE_CHANNELS;
	ar->alignment = RGN_ALIGN_LEFT;
	
	/* only need to set scroll settings, as this will use 'listview' v2d configuration */
	ar->v2d.scroll = V2D_SCROLL_BOTTOM;
	ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;
	
	/* main area */
	ar = MEM_callocN(sizeof(ARegion), "main area for action");
	
	BLI_addtail(&saction->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;
	
	ar->v2d.tot.xmin = -10.0f;
	ar->v2d.tot.ymin = (float)(-sa->winy) / 3.0f;
	ar->v2d.tot.xmax = (float)(sa->winx);
	ar->v2d.tot.ymax = 0.0f;
	
	ar->v2d.cur = ar->v2d.tot;
	
	ar->v2d.min[0] = 0.0f;
	ar->v2d.min[1] = 0.0f;
	
	ar->v2d.max[0] = MAXFRAMEF;
	ar->v2d.max[1] = FLT_MAX;

	ar->v2d.minzoom = 0.01f;
	ar->v2d.maxzoom = 50;
	ar->v2d.scroll = (V2D_SCROLL_BOTTOM | V2D_SCROLL_SCALE_HORIZONTAL);
	ar->v2d.scroll |= (V2D_SCROLL_RIGHT);
	ar->v2d.keepzoom = V2D_LOCKZOOM_Y;
	ar->v2d.keepofs = V2D_KEEPOFS_Y;
	ar->v2d.align = V2D_ALIGN_NO_POS_Y;
	ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;
	
	return (SpaceLink *)saction;
}

/* not spacelink itself */
static void action_free(SpaceLink *UNUSED(sl))
{	
//	SpaceAction *saction = (SpaceAction *) sl;
}


/* spacetype; init callback */
static void action_init(struct wmWindowManager *UNUSED(wm), ScrArea *sa)
{
	SpaceAction *saction = sa->spacedata.first;
	saction->flag |= SACTION_TEMP_NEEDCHANSYNC;
}

static SpaceLink *action_duplicate(SpaceLink *sl)
{
	SpaceAction *sactionn = MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	
	return (SpaceLink *)sactionn;
}



/* add handlers, stuff you only do once or on area/region changes */
static void action_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	/* own keymap */
	keymap = WM_keymap_find(wm->defaultconf, "Dopesheet", SPACE_ACTION, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void action_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceAction *saction = CTX_wm_space_action(C);
	bAnimContext ac;
	View2D *v2d = &ar->v2d;
	View2DGrid *grid;
	View2DScrollers *scrollers;
	short unit = 0, flag = 0;
	
	/* clear and setup matrix */
	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(v2d);
	
	/* time grid */
	unit = (saction->flag & SACTION_DRAWTIME) ? V2D_UNIT_SECONDS : V2D_UNIT_FRAMES;
	grid = UI_view2d_grid_calc(CTX_data_scene(C), v2d, unit, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY, ar->winx, ar->winy);
	UI_view2d_grid_draw(v2d, grid, V2D_GRIDLINES_ALL);
	UI_view2d_grid_free(grid);
	
	ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);

	/* data */
	if (ANIM_animdata_get_context(C, &ac)) {
		draw_channel_strips(&ac, saction, ar);
	}
	
	/* current frame */
	if (saction->flag & SACTION_DRAWTIME) flag |= DRAWCFRA_UNIT_SECONDS;
	if ((saction->flag & SACTION_NODRAWCFRANUM) == 0) flag |= DRAWCFRA_SHOW_NUMBOX;
	ANIM_draw_cfra(C, v2d, flag);
	
	/* markers */
	UI_view2d_view_orthoSpecial(ar, v2d, 1);
	
	flag = (ac.markers && (ac.markers != &ac.scene->markers)) ? DRAW_MARKERS_LOCAL : 0;
	draw_markers_time(C, flag);
	
	/* preview range */
	UI_view2d_view_ortho(v2d);
	ANIM_draw_previewrange(C, v2d, 0);

	/* callback */
	UI_view2d_view_ortho(v2d);
	ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers = UI_view2d_scrollers_calc(C, v2d, unit, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_channel_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	/* ensure the 2d view sync works - main region has bottom scroller */
	ar->v2d.scroll = V2D_SCROLL_BOTTOM;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);
	
	/* own keymap */
	keymap = WM_keymap_find(wm->defaultconf, "Animation Channels", 0, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void action_channel_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	bAnimContext ac;
	View2D *v2d = &ar->v2d;
	
	/* clear and setup matrix */
	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(v2d);
	
	/* data */
	if (ANIM_animdata_get_context(C, &ac)) {
		draw_channel_names((bContext *)C, &ac, ar);
	}
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* no scrollers here */
}


/* add handlers, stuff you only do once or on area/region changes */
static void action_header_area_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void action_header_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void action_channel_area_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_ANIMATION:
			ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			switch (wmn->data) {
				case ND_OB_ACTIVE:
				case ND_FRAME:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_OBJECT:
			switch (wmn->data) {
				case ND_BONE_ACTIVE:
				case ND_BONE_SELECT:
				case ND_KEYS:
					ED_region_tag_redraw(ar);
					break;
				case ND_MODIFIER:
					if (wmn->action == NA_RENAME)
						ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_ID:
			if (wmn->action == NA_RENAME)
				ED_region_tag_redraw(ar);
			break;
		default:
			if (wmn->data == ND_KEYS)
				ED_region_tag_redraw(ar);
			break;
	}
}

static void action_main_area_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_ANIMATION:
			ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			switch (wmn->data) {
				case ND_RENDER_OPTIONS:
				case ND_OB_ACTIVE:
				case ND_FRAME:
				case ND_MARKERS:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_OBJECT:
			switch (wmn->data) {
				case ND_TRANSFORM:
					/* moving object shouldn't need to redraw action */
					break;
				case ND_BONE_ACTIVE:
				case ND_BONE_SELECT:
				case ND_KEYS:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_NODE:
			switch (wmn->action) {
				case NA_EDITED:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_ID:
			if (wmn->action == NA_RENAME)
				ED_region_tag_redraw(ar);
			break;
				
		default:
			if (wmn->data == ND_KEYS)
				ED_region_tag_redraw(ar);
			break;
	}
}

/* editor level listener */
static void action_listener(bScreen *UNUSED(sc), ScrArea *sa, wmNotifier *wmn)
{
	SpaceAction *saction = (SpaceAction *)sa->spacedata.first;
	
	/* context changes */
	switch (wmn->category) {
		case NC_GPENCIL:
			if (wmn->action == NA_EDITED) {
				/* only handle this event in GPencil mode for performance considerations */
				if (saction->mode == SACTCONT_GPENCIL)
					ED_area_tag_redraw(sa);
			}
			break;
		case NC_ANIMATION:
			/* for NLA tweakmode enter/exit, need complete refresh */
			if (wmn->data == ND_NLA_ACTCHANGE) {
				saction->flag |= SACTION_TEMP_NEEDCHANSYNC;
				ED_area_tag_refresh(sa);
			}
			/* for selection changes of animation data, we can just redraw... otherwise autocolor might need to be done again */
			else if (ELEM(wmn->data, ND_KEYFRAME, ND_ANIMCHAN) && (wmn->action == NA_SELECTED))
				ED_area_tag_redraw(sa);
			else
				ED_area_tag_refresh(sa);
			break;
		case NC_SCENE:
			switch (wmn->data) {
				case ND_OB_ACTIVE:  /* selection changed, so force refresh to flush (needs flag set to do syncing) */
				case ND_OB_SELECT:
					saction->flag |= SACTION_TEMP_NEEDCHANSYNC;
					ED_area_tag_refresh(sa);
					break;
					
				default: /* just redrawing the view will do */
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_OBJECT:
			switch (wmn->data) {
				case ND_BONE_SELECT:    /* selection changed, so force refresh to flush (needs flag set to do syncing) */
				case ND_BONE_ACTIVE:
					saction->flag |= SACTION_TEMP_NEEDCHANSYNC;
					ED_area_tag_refresh(sa);
					break;
				case ND_TRANSFORM:
					/* moving object shouldn't need to redraw action */
					break;
				default: /* just redrawing the view will do */
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_MASK:
			if (saction->mode == SACTCONT_MASK) {
				switch (wmn->data) {
					case ND_DATA:
						ED_area_tag_refresh(sa);
						ED_area_tag_redraw(sa);
						break;
					default: /* just redrawing the view will do */
						ED_area_tag_redraw(sa);
						break;
				}
			}
			break;
		case NC_NODE:
			if (wmn->action == NA_SELECTED) {
				/* selection changed, so force refresh to flush (needs flag set to do syncing) */
				saction->flag |= SACTION_TEMP_NEEDCHANSYNC;
				ED_area_tag_refresh(sa);
			}
			break;
		case NC_SPACE:
			switch (wmn->data) {
				case ND_SPACE_DOPESHEET:
					ED_area_tag_redraw(sa);
					break;
				case ND_SPACE_CHANGED:
					saction->flag |= SACTION_TEMP_NEEDCHANSYNC;
					ED_area_tag_refresh(sa);
					break;
			}
			break;
		case NC_WINDOW:
			if (saction->flag & SACTION_TEMP_NEEDCHANSYNC) {
				/* force redraw/refresh after undo/redo - [#28962] */
				ED_area_tag_refresh(sa);
			}
			break;
	}
}

static void action_header_area_listener(bScreen *UNUSED(sc), ScrArea *sa, ARegion *ar, wmNotifier *wmn)
{

	SpaceAction *saction = (SpaceAction *)sa->spacedata.first;

	/* context changes */
	switch (wmn->category) {
		case NC_SCENE:
			switch (wmn->data) {
				case ND_OB_ACTIVE:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_ID:
			if (wmn->action == NA_RENAME)
				ED_region_tag_redraw(ar);
			break;
		case NC_ANIMATION:
			switch (wmn->data) {
				case ND_KEYFRAME:
					saction->flag |= SACTION_TEMP_NEEDCHANSYNC;
					ED_region_tag_redraw(ar);
					break;
			}
			break;
	}

}

static void action_refresh(const bContext *C, ScrArea *sa)
{
	SpaceAction *saction = (SpaceAction *)sa->spacedata.first;
	
	/* update the state of the animchannels in response to changes from the data they represent 
	 * NOTE: the temp flag is used to indicate when this needs to be done, and will be cleared once handled
	 */
	if (saction->flag & SACTION_TEMP_NEEDCHANSYNC) {
		ARegion *ar;
		
		/* Perform syncing of channel state incl. selection
		 * Active action setting also occurs here (as part of anim channel filtering in anim_filter.c)
		 */
		ANIM_sync_animchannels_to_data(C);
		saction->flag &= ~SACTION_TEMP_NEEDCHANSYNC;
		
		/* Tag everything for redraw
		 * - Regions (such as header) need to be manually tagged for redraw too
		 *   or else they don't update [#28962]
		 */
		ED_area_tag_redraw(sa);
		for (ar = sa->regionbase.first; ar; ar = ar->next)
			ED_region_tag_redraw(ar);
	}
	
	/* region updates? */
	// XXX re-sizing y-extents of tot should go here?
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_action(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype action");
	ARegionType *art;
	
	st->spaceid = SPACE_ACTION;
	strncpy(st->name, "Action", BKE_ST_MAXNAME);
	
	st->new = action_new;
	st->free = action_free;
	st->init = action_init;
	st->duplicate = action_duplicate;
	st->operatortypes = action_operatortypes;
	st->keymap = action_keymap;
	st->listener = action_listener;
	st->refresh = action_refresh;
	
	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype action region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init = action_main_area_init;
	art->draw = action_main_area_draw;
	art->listener = action_main_area_listener;
	art->keymapflag = ED_KEYMAP_VIEW2D | ED_KEYMAP_MARKERS | ED_KEYMAP_ANIMATION | ED_KEYMAP_FRAMES;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype action region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
	
	art->init = action_header_area_init;
	art->draw = action_header_area_draw;
	art->listener = action_header_area_listener;
	
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: channels */
	art = MEM_callocN(sizeof(ARegionType), "spacetype action region");
	art->regionid = RGN_TYPE_CHANNELS;
	art->prefsizex = 200;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES;
	
	art->init = action_channel_area_init;
	art->draw = action_channel_area_draw;
	art->listener = action_channel_area_listener;
	
	BLI_addhead(&st->regiontypes, art);
	
	
	BKE_spacetype_register(st);
}

