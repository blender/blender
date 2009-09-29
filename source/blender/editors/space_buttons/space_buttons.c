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
#include "DNA_userdef_types.h"

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

#include "ED_render.h"

#include "buttons_intern.h"	// own include

/* ******************** default callbacks for buttons space ***************** */

static SpaceLink *buttons_new(const bContext *C)
{
	ARegion *ar;
	SpaceButs *sbuts;
	
	sbuts= MEM_callocN(sizeof(SpaceButs), "initbuts");
	sbuts->spacetype= SPACE_BUTS;
	sbuts->align= BUT_AUTO;

	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for buts");
	
	BLI_addtail(&sbuts->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
#if 0
	/* context area */
	ar= MEM_callocN(sizeof(ARegion), "context area for buts");
	BLI_addtail(&sbuts->regionbase, ar);
	ar->regiontype= RGN_TYPE_CHANNELS;
	ar->alignment= RGN_ALIGN_TOP;
#endif

	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for buts");
	
	BLI_addtail(&sbuts->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
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

	if(sbuts->path)
		MEM_freeN(sbuts->path);
}

/* spacetype; init callback */
static void buttons_init(struct wmWindowManager *wm, ScrArea *sa)
{
	SpaceButs *sbuts= sa->spacedata.first;

	/* auto-align based on size */
	if(sbuts->align == BUT_AUTO || !sbuts->align) {
		if(sa->winx > sa->winy)
			sbuts->align= BUT_HORIZONTAL;
		else
			sbuts->align= BUT_VERTICAL;
	}
}

static SpaceLink *buttons_duplicate(SpaceLink *sl)
{
	SpaceButs *sbutsn= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	sbutsn->ri= NULL;
	sbutsn->path= NULL;
	
	return (SpaceLink *)sbutsn;
}

/* add handlers, stuff you only do once or on area/region changes */
static void buttons_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	ED_region_panels_init(wm, ar);

	keymap= WM_keymap_find(wm, "Buttons Generic", SPACE_BUTS, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void buttons_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceButs *sbuts= CTX_wm_space_buts(C);
	int vertical= (sbuts->align == BUT_VERTICAL);

	buttons_context_compute(C, sbuts);

	if(sbuts->mainb == BCONTEXT_SCENE)
		ED_region_panels(C, ar, vertical, "scene", sbuts->mainb);
	else if(sbuts->mainb == BCONTEXT_WORLD)
		ED_region_panels(C, ar, vertical, "world", sbuts->mainb);
	else if(sbuts->mainb == BCONTEXT_OBJECT)
		ED_region_panels(C, ar, vertical, "object", sbuts->mainb);
	else if(sbuts->mainb == BCONTEXT_DATA)
		ED_region_panels(C, ar, vertical, "data", sbuts->mainb);
	else if(sbuts->mainb == BCONTEXT_MATERIAL)
		ED_region_panels(C, ar, vertical, "material", sbuts->mainb);
	else if(sbuts->mainb == BCONTEXT_TEXTURE)
		ED_region_panels(C, ar, vertical, "texture", sbuts->mainb);
	else if(sbuts->mainb == BCONTEXT_PARTICLE)
		ED_region_panels(C, ar, vertical, "particle", sbuts->mainb);
	else if(sbuts->mainb == BCONTEXT_PHYSICS)
		ED_region_panels(C, ar, vertical, "physics", sbuts->mainb);
	else if(sbuts->mainb == BCONTEXT_BONE)
		ED_region_panels(C, ar, vertical, "bone", sbuts->mainb);
	else if(sbuts->mainb == BCONTEXT_MODIFIER)
		ED_region_panels(C, ar, vertical, "modifier", sbuts->mainb);
	else if (sbuts->mainb == BCONTEXT_CONSTRAINT)
		ED_region_panels(C, ar, vertical, "constraint", sbuts->mainb);
	else if(sbuts->mainb == BCONTEXT_BONE_CONSTRAINT)
		ED_region_panels(C, ar, vertical, "bone_constraint", sbuts->mainb);

    sbuts->re_align= 0;
	sbuts->mainbo= sbuts->mainb;
}

void buttons_operatortypes(void)
{
	WM_operatortype_append(BUTTONS_OT_toolbox);
	WM_operatortype_append(BUTTONS_OT_file_browse);
}

void buttons_keymap(struct wmWindowManager *wm)
{
	wmKeyMap *keymap= WM_keymap_find(wm, "Buttons Generic", SPACE_BUTS, 0);
	
	WM_keymap_add_item(keymap, "BUTTONS_OT_toolbox", RIGHTMOUSE, KM_PRESS, 0, 0);
}

//#define PY_HEADER
/* add handlers, stuff you only do once or on area/region changes */
static void buttons_header_area_init(wmWindowManager *wm, ARegion *ar)
{
#ifdef PY_HEADER
	ED_region_header_init(ar);
#else
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);
#endif
}

static void buttons_header_area_draw(const bContext *C, ARegion *ar)
{
#ifdef PY_HEADER
	ED_region_header(C, ar);
#else

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
#endif	

	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

/* reused! */
static void buttons_area_listener(ScrArea *sa, wmNotifier *wmn)
{
	SpaceButs *sbuts= sa->spacedata.first;

	/* context changes */
	switch(wmn->category) {
		case NC_SCENE:
			/* lazy general redraw tag here, in case more than 1 propertie window is opened 
			Not all RNA props have a ND_sub notifier(yet) */
			ED_area_tag_redraw(sa);
			switch(wmn->data) {
				case ND_FRAME:
				case ND_MODE:
				case ND_RENDER_OPTIONS:
					ED_area_tag_redraw(sa);
					break;
					
				case ND_OB_ACTIVE:
					ED_area_tag_redraw(sa);
					sbuts->preview= 1;
					break;
			}
			break;
		case NC_OBJECT:
			ED_area_tag_redraw(sa);
			/* lazy general redraw tag here, in case more than 1 propertie window is opened 
			Not all RNA props have a ND_ notifier(yet) */
			switch(wmn->data) {
				case ND_TRANSFORM:
				case ND_BONE_ACTIVE:
				case ND_BONE_SELECT:
				case ND_MODIFIER:
				case ND_CONSTRAINT:
					ED_area_tag_redraw(sa);
					break;
				case ND_SHADING:
				case ND_SHADING_DRAW:
					/* currently works by redraws... if preview is set, it (re)starts job */
					sbuts->preview= 1;
					break;
			}
			break;
		case NC_GEOM:
			switch(wmn->data) {
				case ND_SELECT:
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_MATERIAL:
			ED_area_tag_redraw(sa);
			switch(wmn->data) {
				case ND_SHADING:
				case ND_SHADING_DRAW:
					/* currently works by redraws... if preview is set, it (re)starts job */
					sbuts->preview= 1;
					break;
			}					
			break;
		case NC_WORLD:
		case NC_LAMP:
		case NC_TEXTURE:
			ED_area_tag_redraw(sa);
			sbuts->preview= 1;
			break;
		case NC_SPACE:
			if(wmn->data == ND_SPACE_PROPERTIES)
				ED_area_tag_redraw(sa);
			break;
	}

	if(wmn->data == ND_KEYS)
		ED_area_tag_redraw(sa);
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
	st->listener= buttons_area_listener;
	st->context= buttons_context;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype buttons region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= buttons_main_area_init;
	art->draw= buttons_main_area_draw;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_FRAMES;
	BLI_addhead(&st->regiontypes, art);

	buttons_context_register(art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype buttons region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= BUTS_HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES;
	
	art->init= buttons_header_area_init;
	art->draw= buttons_header_area_draw;
	BLI_addhead(&st->regiontypes, art);

	BKE_spacetype_register(st);
}

