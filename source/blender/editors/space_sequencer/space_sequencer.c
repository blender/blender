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
#include "BKE_sequencer.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_markers.h"

#include "sequencer_intern.h"	// own include

/* ******************** manage regions ********************* */

ARegion *sequencer_has_buttons_region(ScrArea *sa)
{
	ARegion *ar, *arnew;
	
	for(ar= sa->regionbase.first; ar; ar= ar->next)
		if(ar->regiontype==RGN_TYPE_UI)
			return ar;
	
	/* add subdiv level; after header */
	for(ar= sa->regionbase.first; ar; ar= ar->next)
		if(ar->regiontype==RGN_TYPE_HEADER)
			break;
	
	/* is error! */
	if(ar==NULL) return NULL;
	
	arnew= MEM_callocN(sizeof(ARegion), "buttons for sequencer");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype= RGN_TYPE_UI;
	arnew->alignment= RGN_ALIGN_RIGHT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}

ARegion *sequencer_find_region(ScrArea *sa, short type)
{
	ARegion *ar=NULL;
	
	for(ar= sa->regionbase.first; ar; ar= ar->next)
		if(ar->regiontype==type)
			return ar;

	return ar;
}

void ED_sequencer_update_view(bContext *C, int view)
{
	ScrArea *sa= CTX_wm_area(C);
	
	ARegion *ar_main= sequencer_find_region(sa, RGN_TYPE_WINDOW);
	ARegion *ar_preview= sequencer_find_region(sa, RGN_TYPE_PREVIEW);

	switch (view) {
		case SEQ_VIEW_SEQUENCE:
			if (ar_main->flag & RGN_FLAG_HIDDEN) {
				ar_main->flag &= ~RGN_FLAG_HIDDEN;
				ar_main->v2d.flag &= ~V2D_IS_INITIALISED;
			}
			if (!(ar_preview->flag & RGN_FLAG_HIDDEN)) {
				ar_preview->flag |= RGN_FLAG_HIDDEN;
				ar_preview->v2d.flag &= ~V2D_IS_INITIALISED;
				WM_event_remove_handlers(C, &ar_preview->handlers);
			}
			ar_main->alignment= RGN_ALIGN_NONE;
			ar_preview->alignment= RGN_ALIGN_NONE;
			break;
		case SEQ_VIEW_PREVIEW:
			if (!(ar_main->flag & RGN_FLAG_HIDDEN)) {
				ar_main->flag |= RGN_FLAG_HIDDEN;
				ar_main->v2d.flag &= ~V2D_IS_INITIALISED;
				WM_event_remove_handlers(C, &ar_main->handlers);
			}
			if (ar_preview->flag & RGN_FLAG_HIDDEN) {
				ar_preview->flag &= ~RGN_FLAG_HIDDEN;
				ar_preview->v2d.flag &= ~V2D_IS_INITIALISED;
			}
			ar_main->alignment= RGN_ALIGN_NONE;
			ar_preview->alignment= RGN_ALIGN_NONE;
			break;
		case SEQ_VIEW_SEQUENCE_PREVIEW:
			if (ar_main->flag & RGN_FLAG_HIDDEN) {
				ar_main->flag &= ~RGN_FLAG_HIDDEN;
				ar_main->v2d.flag &= ~V2D_IS_INITIALISED;
			}
			if (ar_preview->flag & RGN_FLAG_HIDDEN) {
				ar_preview->flag &= ~RGN_FLAG_HIDDEN;
				ar_preview->v2d.flag &= ~V2D_IS_INITIALISED;
			}
			ar_main->alignment= RGN_ALIGN_NONE;
			ar_preview->alignment= RGN_ALIGN_TOP;
			break;
	}

	ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), sa);
	ED_area_tag_redraw(sa);
}


/* ******************** default callbacks for sequencer space ***************** */

static SpaceLink *sequencer_new(const bContext *C)
{
	Scene *scene= CTX_data_scene(C);
	ARegion *ar;
	SpaceSeq *sseq;
	
	sseq= MEM_callocN(sizeof(SpaceSeq), "initsequencer");
	sseq->spacetype= SPACE_SEQ;
	sseq->zoom= 4;
	sseq->chanshown = 0;
	sseq->view = SEQ_VIEW_SEQUENCE;
	sseq->mainb = SEQ_DRAW_IMG_IMBUF;
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for sequencer");
	
	BLI_addtail(&sseq->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* buttons/list view */
	ar= MEM_callocN(sizeof(ARegion), "buttons for sequencer");
	
	BLI_addtail(&sseq->regionbase, ar);
	ar->regiontype= RGN_TYPE_UI;
	ar->alignment= RGN_ALIGN_RIGHT;
	ar->flag = RGN_FLAG_HIDDEN;
	
	/* preview area */
	ar= MEM_callocN(sizeof(ARegion), "preview area for sequencer");
	BLI_addtail(&sseq->regionbase, ar);
	ar->regiontype= RGN_TYPE_PREVIEW;
	ar->alignment= RGN_ALIGN_TOP;
	ar->flag |= RGN_FLAG_HIDDEN;

	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for sequencer");
	
	BLI_addtail(&sseq->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	
	/* seq space goes from (0,8) to (0, efra) */
	
	ar->v2d.tot.xmin= 0.0f;
	ar->v2d.tot.ymin= 0.0f;
	ar->v2d.tot.xmax= scene->r.efra;
	ar->v2d.tot.ymax= 8.0f;
	
	ar->v2d.cur= ar->v2d.tot;
	
	ar->v2d.min[0]= 10.0f;
	ar->v2d.min[1]= 4.0f;
	
	ar->v2d.max[0]= MAXFRAMEF;
	ar->v2d.max[1]= MAXSEQ;
	
	ar->v2d.minzoom= 0.01f;
	ar->v2d.maxzoom= 100.0f;
	
	ar->v2d.scroll |= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
	ar->v2d.scroll |= (V2D_SCROLL_LEFT|V2D_SCROLL_SCALE_VERTICAL);
	ar->v2d.keepzoom= 0;
	ar->v2d.keeptot= 0;
	ar->v2d.align= V2D_ALIGN_NO_NEG_Y;

	return (SpaceLink *)sseq;
}

/* not spacelink itself */
static void sequencer_free(SpaceLink *sl)
{	
//	SpaceSeq *sseq= (SpaceSequencer*) sl;
	
// XXX	if(sseq->gpd) free_gpencil_data(sseq->gpd);

}


/* spacetype; init callback */
static void sequencer_init(struct wmWindowManager *wm, ScrArea *sa)
{

}

static SpaceLink *sequencer_duplicate(SpaceLink *sl)
{
	SpaceSeq *sseqn= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
// XXX	sseq->gpd= gpencil_data_duplicate(sseq->gpd);

	return (SpaceLink *)sseqn;
}


/* *********************** sequencer (main) region ************************ */
/* add handlers, stuff you only do once or on area/region changes */
static void sequencer_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	keymap= WM_keymap_find(wm->defaultconf, "SequencerCommon", SPACE_SEQ, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
	
	/* own keymap */
	keymap= WM_keymap_find(wm->defaultconf, "Sequencer", SPACE_SEQ, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void sequencer_main_area_draw(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	
	/* NLE - strip editing timeline interface */
	draw_timeline_seq(C, ar);
}


/* add handlers, stuff you only do once or on area/region changes */
static void sequencer_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	ED_region_header_init(ar);
}

static void sequencer_header_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void sequencer_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_SCENE:
			switch(wmn->data) {
				case ND_FRAME:
				case ND_MARKERS:
				case ND_SEQUENCER:
				case ND_SEQUENCER_SELECT:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_SPACE:
			if(wmn->data == ND_SPACE_SEQUENCER)
				ED_region_tag_redraw(ar);
			break;
		case NC_ID:
			if(wmn->action == NA_RENAME)
				ED_region_tag_redraw(ar);
			break;
	}
}

/* *********************** preview region ************************ */
static void sequencer_preview_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_STANDARD, ar->winx, ar->winy);
	
	keymap= WM_keymap_find(wm->defaultconf, "SequencerCommon", SPACE_SEQ, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

	/* own keymap */
	keymap= WM_keymap_find(wm->defaultconf, "SequencerPreview", SPACE_SEQ, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void sequencer_preview_area_draw(const bContext *C, ARegion *ar)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceSeq *sseq= sa->spacedata.first;
	Scene *scene= CTX_data_scene(C);
	
	/* XXX temp fix for wrong setting in sseq->mainb */
	if (sseq->mainb == SEQ_DRAW_SEQUENCE) sseq->mainb = SEQ_DRAW_IMG_IMBUF;
	draw_image_seq(scene, ar, sseq);
}

static void sequencer_preview_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_SCENE:
			switch(wmn->data) {
				case ND_FRAME:
				case ND_MARKERS:
				case ND_SEQUENCER:
				case ND_SEQUENCER_SELECT:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_SPACE:
			if(wmn->data == ND_SPACE_SEQUENCER)
				ED_region_tag_redraw(ar);
			break;
		case NC_ID:
			switch(wmn->data) {
				case NA_RENAME:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
	}
}

/* *********************** buttons region ************************ */

/* add handlers, stuff you only do once or on area/region changes */
static void sequencer_buttons_area_init(wmWindowManager *wm, ARegion *ar)
{
	
	ED_region_panels_init(wm, ar);
	
}

static void sequencer_buttons_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, NULL, -1);
}

static void sequencer_buttons_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_SCENE:
		switch(wmn->data) {
			case ND_FRAME:
			case ND_SEQUENCER:
			case ND_SEQUENCER_SELECT:
				ED_region_tag_redraw(ar);
				break;
		}
		break;
		case NC_SPACE:
			if(wmn->data == ND_SPACE_SEQUENCER)
				ED_region_tag_redraw(ar);
			break;
		case NC_ID:
			if(wmn->action == NA_RENAME)
				ED_region_tag_redraw(ar);
			break;
	}
}
/* ************************************* */

/* only called once, from space/spacetypes.c */
void ED_spacetype_sequencer(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype sequencer");
	ARegionType *art;
	
	st->spaceid= SPACE_SEQ;
	
	st->new= sequencer_new;
	st->free= sequencer_free;
	st->init= sequencer_init;
	st->duplicate= sequencer_duplicate;
	st->operatortypes= sequencer_operatortypes;
	st->keymap= sequencer_keymap;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype sequencer region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= sequencer_main_area_init;
	art->draw= sequencer_main_area_draw;
	art->listener= sequencer_main_area_listener;
	art->keymapflag= ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES|ED_KEYMAP_ANIMATION;

	BLI_addhead(&st->regiontypes, art);

	/* preview */
	art= MEM_callocN(sizeof(ARegionType), "spacetype sequencer region");
	art->regionid = RGN_TYPE_PREVIEW;
	art->minsizey = 240; // XXX
	art->init= sequencer_preview_area_init;
	art->draw= sequencer_preview_area_draw;
	art->listener= sequencer_preview_area_listener;
	art->keymapflag= ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES|ED_KEYMAP_ANIMATION;
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: listview/buttons */
	art= MEM_callocN(sizeof(ARegionType), "spacetype sequencer region");
	art->regionid = RGN_TYPE_UI;
	art->minsizex= 220; // XXX
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_FRAMES;
	art->listener= sequencer_buttons_area_listener;
	art->init= sequencer_buttons_area_init;
	art->draw= sequencer_buttons_area_draw;
	BLI_addhead(&st->regiontypes, art);
	
	/* Keep as python only for now
	sequencer_buttons_register(art);
	*/

	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype sequencer region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES|ED_KEYMAP_HEADER;
	
	art->init= sequencer_header_area_init;
	art->draw= sequencer_header_area_draw;
	art->listener= sequencer_main_area_listener;
	
	BLI_addhead(&st->regiontypes, art);
	
	BKE_spacetype_register(st);
}

