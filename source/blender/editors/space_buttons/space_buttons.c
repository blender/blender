/**
 * $Id$
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

#include <string.h>
#include <stdio.h>


#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

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

	keymap= WM_keymap_find(wm->defaultconf, "Property Editor", SPACE_BUTS, 0);
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
	else if(sbuts->mainb == BCONTEXT_RENDER)
		ED_region_panels(C, ar, vertical, "render", sbuts->mainb);
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

void buttons_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap= WM_keymap_find(keyconf, "Property Editor", SPACE_BUTS, 0);
	
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


	/* clear */
	UI_ThemeClearColor(ED_screen_area_active(C)?TH_HEADER:TH_HEADERDESEL);
	glClear(GL_COLOR_BUFFER_BIT);
	
	/* set view2d view matrix for scrolling (without scrollers) */
	UI_view2d_view_ortho(C, &ar->v2d);
	
	buttons_header_buttons(C, ar);
#endif	

	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

/* draw a certain button set only if properties area is currently
 * showing that button set, to reduce unnecessary drawing. */
static void buttons_area_redraw(ScrArea *sa, short buttons)
{
	SpaceButs *sbuts= sa->spacedata.first;
	
	/* if the area's current button set is equal to the one to redraw */
	if(sbuts->mainb == buttons)
		ED_area_tag_redraw(sa);
}

/* reused! */
static void buttons_area_listener(ScrArea *sa, wmNotifier *wmn)
{
	SpaceButs *sbuts= sa->spacedata.first;

	/* context changes */
	switch(wmn->category) {
		case NC_SCENE:
			switch(wmn->data) {
				case ND_RENDER_OPTIONS:
					buttons_area_redraw(sa, BCONTEXT_RENDER);
					break;
				case ND_FRAME:
					buttons_area_redraw(sa, BCONTEXT_RENDER);
					buttons_area_redraw(sa, BCONTEXT_MATERIAL);
					buttons_area_redraw(sa, BCONTEXT_TEXTURE);
					buttons_area_redraw(sa, BCONTEXT_WORLD);
					buttons_area_redraw(sa, BCONTEXT_DATA);
					buttons_area_redraw(sa, BCONTEXT_PHYSICS);
					sbuts->preview= 1;
					break;
				case ND_OB_ACTIVE:
					ED_area_tag_redraw(sa);
					sbuts->preview= 1;
					break;
				case ND_KEYINGSET:
					buttons_area_redraw(sa, BCONTEXT_SCENE);
					break;
				case ND_MODE:
				case ND_LAYER:
				default:
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_OBJECT:
			switch(wmn->data) {
				case ND_TRANSFORM:
					buttons_area_redraw(sa, BCONTEXT_OBJECT);
					break;
				case ND_POSE:
				case ND_BONE_ACTIVE:
				case ND_BONE_SELECT:
					buttons_area_redraw(sa, BCONTEXT_BONE);
					buttons_area_redraw(sa, BCONTEXT_BONE_CONSTRAINT);
					break;
				case ND_MODIFIER:
					if(wmn->action == NA_RENAME)
						ED_area_tag_redraw(sa);
					else
						buttons_area_redraw(sa, BCONTEXT_MODIFIER);
						buttons_area_redraw(sa, BCONTEXT_PHYSICS);
					break;
				case ND_CONSTRAINT:
					buttons_area_redraw(sa, BCONTEXT_CONSTRAINT);
					buttons_area_redraw(sa, BCONTEXT_BONE_CONSTRAINT);
					break;
				case ND_PARTICLE_DATA:
					buttons_area_redraw(sa, BCONTEXT_PARTICLE);
					break;
				case ND_DRAW:
					buttons_area_redraw(sa, BCONTEXT_OBJECT);
					buttons_area_redraw(sa, BCONTEXT_DATA);
					buttons_area_redraw(sa, BCONTEXT_PHYSICS);
				case ND_SHADING:
				case ND_SHADING_DRAW:
					/* currently works by redraws... if preview is set, it (re)starts job */
					sbuts->preview= 1;
					break;
				default:
					/* Not all object RNA props have a ND_ notifier (yet) */
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_GEOM:
			switch(wmn->data) {
				case ND_SELECT:
				case ND_DATA:
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_MATERIAL:
			ED_area_tag_redraw(sa);
			switch(wmn->data) {
				case ND_SHADING:
				case ND_SHADING_DRAW:
				case ND_NODES:
					/* currently works by redraws... if preview is set, it (re)starts job */
					sbuts->preview= 1;
					break;
			}					
			break;
		case NC_WORLD:
			buttons_area_redraw(sa, BCONTEXT_WORLD);
			sbuts->preview= 1;
			break;
		case NC_LAMP:
			buttons_area_redraw(sa, BCONTEXT_DATA);
			sbuts->preview= 1;
			break;
		case NC_BRUSH:
			buttons_area_redraw(sa, BCONTEXT_TEXTURE);
			sbuts->preview= 1;
			break;
		case NC_TEXTURE:
		case NC_IMAGE:
			ED_area_tag_redraw(sa);
			sbuts->preview= 1;
			break;
		case NC_SPACE:
			if(wmn->data == ND_SPACE_PROPERTIES)
				ED_area_tag_redraw(sa);
			break;
		case NC_ID:
			if(wmn->action == NA_RENAME)
				ED_area_tag_redraw(sa);
			break;
		case NC_ANIMATION:
			switch(wmn->data) {
				case ND_KEYFRAME_EDIT:
					ED_area_tag_redraw(sa);
					break;
			}
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
	strncpy(st->name, "Buttons", BKE_ST_MAXNAME);
	
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
	art->prefsizey= BUTS_HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES|ED_KEYMAP_HEADER;
	
	art->init= buttons_header_area_init;
	art->draw= buttons_header_area_draw;
	BLI_addhead(&st->regiontypes, art);

	BKE_spacetype_register(st);
}

