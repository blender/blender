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
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_previewrender.h"

#include "buttons_intern.h"	// own include

/* ******************** default callbacks for buttons space ***************** */

static SpaceLink *buttons_new(const bContext *C)
{
	ARegion *ar;
	SpaceButs *sbuts;
	
	sbuts= MEM_callocN(sizeof(SpaceButs), "initbuts");
	sbuts->spacetype= SPACE_BUTS;
	sbuts->scaflag= BUTS_SENS_LINK|BUTS_SENS_ACT|BUTS_CONT_ACT|BUTS_ACT_ACT|BUTS_ACT_LINK;

	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for buts");
	
	BLI_addtail(&sbuts->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for buts");
	
	BLI_addtail(&sbuts->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
#if 0 // disabled, as this currently draws badly in new system
	/* buts space goes from (0,0) to (1280, 228) */
	ar->v2d.tot.xmin= 0.0f;
	ar->v2d.tot.ymin= 0.0f;
	ar->v2d.tot.xmax= 1279.0f;
	ar->v2d.tot.ymax= 228.0f;
	
	ar->v2d.cur= sbuts->v2d.tot;
	
	ar->v2d.min[0]= 256.0f;
	ar->v2d.min[1]= 42.0f;
	
	ar->v2d.max[0]= 2048.0f;
	ar->v2d.max[1]= 450.0f;
	
	ar->v2d.minzoom= 0.5f;
	ar->v2d.maxzoom= 1.21f;
	
	ar->v2d.scroll= 0;  // TODO: will we need scrollbars?
	ar->v2d.align= V2D_ALIGN_NO_NEG_X|V2D_ALIGN_NO_NEG_Y;
	ar->v2d.keepzoom= V2D_KEEPZOOM|V2D_KEEPASPECT;
	ar->v2d.keeptot= V2D_KEEPTOT_BOUNDS;
#endif	
	
	
	return (SpaceLink *)sbuts;
}

/* not spacelink itself */
static void buttons_free(SpaceLink *sl)
{	
	SpaceButs *sbuts= (SpaceButs*) sl;
	
	if(sbuts->ri) { 
		if (sbuts->ri->rect) MEM_freeN(sbuts->ri->rect);
		MEM_freeN(sbuts->ri);
	}
	
}


/* spacetype; init callback */
static void buttons_init(struct wmWindowManager *wm, ScrArea *sa)
{

}

static SpaceLink *buttons_duplicate(SpaceLink *sl)
{
	SpaceButs *sbutsn= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	sbutsn->ri= NULL;
	
	return (SpaceLink *)sbutsn;
}

/* add handlers, stuff you only do once or on area/region changes */
static void buttons_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	ListBase *keymap;

//	ar->v2d.minzoom= ar->v2d.maxzoom= 1.0f;
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_PANELS_UI, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_listbase(wm, "Buttons", SPACE_BUTS, 0);	/* XXX weak? */
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void buttons_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceButs *sbuts= (SpaceButs*)CTX_wm_space_data(C);

	if(sbuts->mainb == CONTEXT_OBJECT) {
		int tab= sbuts->tab[CONTEXT_OBJECT];
		int vertical= (sbuts->align == 2);

		if(tab == TAB_OBJECT_OBJECT)
			uiRegionPanelLayout(C, ar, vertical, "object");
	}
	else if (sbuts->mainb == CONTEXT_SCENE){
		int tab= sbuts->tab[CONTEXT_SCENE];
		int vertical= (sbuts->align == 2);

		if(tab == TAB_SCENE_RENDER)
			uiRegionPanelLayout(C, ar, vertical, "render");
	}	
	else {
		View2D *v2d= &ar->v2d;
		float col[3], fac;

		/* clear and setup matrix */
		UI_GetThemeColor3fv(TH_BACK, col);
		glClearColor(col[0], col[1], col[2], 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		
		UI_view2d_view_ortho(C, v2d);

		/* swapbuffers indicator */
		fac= BLI_frand();
		glColor3f(fac, fac, fac);
		glRecti(20, v2d->cur.ymin+2,  30, v2d->cur.ymin+12);
		
		/* reset view matrix */
		UI_view2d_view_restore(C);
		
		/* scrollers? */
	}

    sbuts->re_align= 0;
	sbuts->mainbo= sbuts->mainb;
	sbuts->tabo= sbuts->tab[sbuts->mainb];
}

void buttons_operatortypes(void)
{
	
}

void buttons_keymap(struct wmWindowManager *wm)
{
	
}

/* add handlers, stuff you only do once or on area/region changes */
static void buttons_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);
}

static void buttons_header_area_draw(const bContext *C, ARegion *ar)
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
	
	buttons_header_buttons(C, ar);
	
	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

/* reused! */
static void buttons_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_SCENE:
			switch(wmn->data) {
				case ND_FRAME:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
	}
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_buttons(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype buttons");
	ARegionType *art;
	
	st->spaceid= SPACE_BUTS;
	
	st->new= buttons_new;
	st->free= buttons_free;
	st->init= buttons_init;
	st->duplicate= buttons_duplicate;
	st->operatortypes= buttons_operatortypes;
	st->keymap= buttons_keymap;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype buttons region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= buttons_main_area_init;
	art->draw= buttons_main_area_draw;
	art->listener= buttons_area_listener;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES;

	buttons_object_register(art);
	buttons_scene_register(art);

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype buttons region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES;
	
	art->init= buttons_header_area_init;
	art->draw= buttons_header_area_draw;
	art->listener= buttons_area_listener;
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: channels */
	art= MEM_callocN(sizeof(ARegionType), "spacetype buttons region");
	art->regionid = RGN_TYPE_CHANNELS;
	art->minsizex= 80;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;
	
//	art->init= buttons_channel_area_init;
//	art->draw= buttons_channel_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	BKE_spacetype_register(st);
}

