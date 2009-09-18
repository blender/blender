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

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_anim_api.h"
#include "ED_markers.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "graph_intern.h"	// own include

/* ******************** manage regions ********************* */

ARegion *graph_has_buttons_region(ScrArea *sa)
{
	ARegion *ar, *arnew;
	
	for (ar= sa->regionbase.first; ar; ar= ar->next) {
		if (ar->regiontype==RGN_TYPE_UI)
			return ar;
	}
	
	/* add subdiv level; after main */
	for (ar= sa->regionbase.first; ar; ar= ar->next) {
		if (ar->regiontype==RGN_TYPE_WINDOW)
			break;
	}
	
	/* is error! */
	if (ar==NULL) return NULL;
	
	arnew= MEM_callocN(sizeof(ARegion), "buttons for graph");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype= RGN_TYPE_UI;
	arnew->alignment= RGN_ALIGN_RIGHT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}


/* ******************** default callbacks for ipo space ***************** */

static SpaceLink *graph_new(const bContext *C)
{
	Scene *scene= CTX_data_scene(C);
	ARegion *ar;
	SpaceIpo *sipo;
	
	/* Graph Editor - general stuff */
	sipo= MEM_callocN(sizeof(SpaceIpo), "init graphedit");
	sipo->spacetype= SPACE_IPO;
	
	sipo->autosnap= SACTSNAP_FRAME;
	
	/* allocate DopeSheet data for Graph Editor */
	sipo->ads= MEM_callocN(sizeof(bDopeSheet), "GraphEdit DopeSheet");
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for graphedit");
	
	BLI_addtail(&sipo->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* channels */
	ar= MEM_callocN(sizeof(ARegion), "channels area for graphedit");
	
	BLI_addtail(&sipo->regionbase, ar);
	ar->regiontype= RGN_TYPE_CHANNELS;
	ar->alignment= RGN_ALIGN_LEFT;
	
	ar->v2d.scroll = (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM);
	
	/* ui buttons */
	ar= MEM_callocN(sizeof(ARegion), "buttons area for graphedit");
	
	BLI_addtail(&sipo->regionbase, ar);
	ar->regiontype= RGN_TYPE_UI;
	ar->alignment= RGN_ALIGN_RIGHT;
	ar->flag = RGN_FLAG_HIDDEN;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for graphedit");
	
	BLI_addtail(&sipo->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	ar->v2d.tot.xmin= 0.0f;
	ar->v2d.tot.ymin= (float)scene->r.sfra - 10.0f;
	ar->v2d.tot.xmax= (float)scene->r.efra;
	ar->v2d.tot.ymax= 10.0f;
	
	ar->v2d.cur= ar->v2d.tot;
	
	ar->v2d.min[0]= 0.01f;
	ar->v2d.min[1]= 0.01f;
	
	ar->v2d.max[0]= MAXFRAMEF;
	ar->v2d.max[1]= 50000.0f;
	
	ar->v2d.scroll= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
	ar->v2d.scroll |= (V2D_SCROLL_LEFT|V2D_SCROLL_SCALE_VERTICAL);
	
	ar->v2d.keeptot= 0;
	
	return (SpaceLink *)sipo;
}

/* not spacelink itself */
static void graph_free(SpaceLink *sl)
{	
	SpaceIpo *si= (SpaceIpo *)sl;
	
	if (si->ads) {
		BLI_freelistN(&si->ads->chanbase);
		MEM_freeN(si->ads);
	}
	
	if (si->ghostCurves.first)
		free_fcurves(&si->ghostCurves);
}


/* spacetype; init callback */
static void graph_init(struct wmWindowManager *wm, ScrArea *sa)
{
	SpaceIpo *sipo= (SpaceIpo *)sa->spacedata.first;
	
	/* init dopesheet data if non-existant (i.e. for old files) */
	if (sipo->ads == NULL)
		sipo->ads= MEM_callocN(sizeof(bDopeSheet), "GraphEdit DopeSheet");

	ED_area_tag_refresh(sa);
}

static SpaceLink *graph_duplicate(SpaceLink *sl)
{
	SpaceIpo *sipon= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	BLI_duplicatelist(&sipon->ghostCurves, &((SpaceIpo *)sl)->ghostCurves);
	sipon->ads= MEM_dupallocN(sipon->ads);
	
	return (SpaceLink *)sipon;
}

/* add handlers, stuff you only do once or on area/region changes */
static void graph_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_find(wm, "GraphEdit Keys", SPACE_IPO, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
	keymap= WM_keymap_find(wm, "GraphEdit Generic", SPACE_IPO, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void graph_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceIpo *sipo= CTX_wm_space_graph(C);
	bAnimContext ac;
	View2D *v2d= &ar->v2d;
	View2DGrid *grid;
	View2DScrollers *scrollers;
	float col[3];
	short unitx=0, unity=V2D_UNIT_VALUES, flag=0;
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(C, v2d);
	
	/* grid */
	unitx= (sipo->flag & SIPO_DRAWTIME)? V2D_UNIT_SECONDS : V2D_UNIT_FRAMESCALE;
	grid= UI_view2d_grid_calc(C, v2d, unitx, V2D_GRID_NOCLAMP, unity, V2D_GRID_NOCLAMP, ar->winx, ar->winy);
	UI_view2d_grid_draw(C, v2d, grid, V2D_GRIDLINES_ALL);
	
	/* draw data */
	if (ANIM_animdata_get_context(C, &ac)) {
		/* draw ghost curves */
		graph_draw_ghost_curves(&ac, sipo, ar, grid);
		
		/* draw curves twice - unselected, then selected, so that the are fewer occlusion problems */
		graph_draw_curves(&ac, sipo, ar, grid, 0);
		graph_draw_curves(&ac, sipo, ar, grid, 1);
		
		/* XXX the slow way to set tot rect... but for nice sliders needed (ton) */
		get_graph_keyframe_extents(&ac, &v2d->tot.xmin, &v2d->tot.xmax, &v2d->tot.ymin, &v2d->tot.ymax);
		/* extra offset so that these items are visible */
		v2d->tot.xmin -= 10.0f;
		v2d->tot.xmax += 10.0f;
	}
	
	/* only free grid after drawing data, as we need to use it to determine sampling rate */
	UI_view2d_grid_free(grid);
	
	/* current frame */
	if (sipo->flag & SIPO_DRAWTIME) 	flag |= DRAWCFRA_UNIT_SECONDS;
	if ((sipo->flag & SIPO_NODRAWCFRANUM)==0)  flag |= DRAWCFRA_SHOW_NUMBOX;
	ANIM_draw_cfra(C, v2d, flag);
	
	/* markers */
	UI_view2d_view_orthoSpecial(C, v2d, 1);
	draw_markers_time(C, 0);
	
	/* preview range */
	UI_view2d_view_ortho(C, v2d);
	ANIM_draw_previewrange(C, v2d);
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
		// FIXME: args for scrollers depend on the type of data being shown...
	scrollers= UI_view2d_scrollers_calc(C, v2d, unitx, V2D_GRID_NOCLAMP, unity, V2D_GRID_NOCLAMP);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

static void graph_channel_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_find(wm, "Animation_Channels", 0, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
	keymap= WM_keymap_find(wm, "GraphEdit Generic", SPACE_IPO, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void graph_channel_area_draw(const bContext *C, ARegion *ar)
{
	SpaceIpo *sipo= CTX_wm_space_graph(C);
	bAnimContext ac;
	View2D *v2d= &ar->v2d;
	View2DScrollers *scrollers;
	float col[3];
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(C, v2d);
	
	/* draw channels */
	if (ANIM_animdata_get_context(C, &ac)) {
		graph_draw_channel_names((bContext*)C, &ac, sipo, ar);
	}
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers= UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

/* add handlers, stuff you only do once or on area/region changes */
static void graph_header_area_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_HEADER, ar->winx, ar->winy);
}

static void graph_header_area_draw(const bContext *C, ARegion *ar)
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
	
	graph_header_buttons(C, ar);
	
	/* restore view matrix? */
	UI_view2d_view_restore(C);
}

/* add handlers, stuff you only do once or on area/region changes */
static void graph_buttons_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	ED_region_panels_init(wm, ar);

	keymap= WM_keymap_find(wm, "GraphEdit Generic", SPACE_IPO, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void graph_buttons_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, NULL, -1);
}

static void graph_region_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_ANIMATION:
			ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			switch(wmn->data) {
				case ND_OB_ACTIVE:
				case ND_FRAME:
				case ND_MARKERS:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_OBJECT:
			switch(wmn->data) {
				case ND_BONE_ACTIVE:
				case ND_BONE_SELECT:
				case ND_KEYS:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		default:
			if(wmn->data==ND_KEYS)
				ED_region_tag_redraw(ar);
				
	}
}

/* editor level listener */
static void graph_listener(ScrArea *sa, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_ANIMATION:
			ED_area_tag_refresh(sa);
			break;
		case NC_SCENE:
			/*switch (wmn->data) {
				case ND_OB_ACTIVE:
				case ND_OB_SELECT:
					ED_area_tag_refresh(sa);
					break;
			}*/
			ED_area_tag_refresh(sa);
			break;
		case NC_OBJECT:
			/*switch (wmn->data) {
				case ND_BONE_SELECT:
				case ND_BONE_ACTIVE:
					ED_area_tag_refresh(sa);
					break;
			}*/
			ED_area_tag_refresh(sa);
			break;
		case NC_SPACE:
			if(wmn->data == ND_SPACE_GRAPH)
				ED_area_tag_redraw(sa);
			break;
		default:
			if(wmn->data==ND_KEYS)
				ED_area_tag_refresh(sa);
	}
}



static void graph_refresh(const bContext *C, ScrArea *sa)
{
	SpaceIpo *sipo = (SpaceIpo *)sa->spacedata.first;
	bAnimContext ac;
	
	/* updates to data needed depends on Graph Editor mode... */
	switch (sipo->mode) {
		case SIPO_MODE_ANIMATION: /* all animation */
		{
			
		}
			break;
		
		case SIPO_MODE_DRIVERS: /* drivers only  */
		{
		
		}
			break; 
	}
	
	/* region updates? */
	// XXX resizing y-extents of tot should go here?
	
	/* init/adjust F-Curve colors */
	if (ANIM_animdata_get_context(C, &ac)) {
		ListBase anim_data = {NULL, NULL};
		bAnimListElem *ale;
		int filter;
		int items, i;
		
		/* build list of F-Curves which will be visible as channels in channel-region
		 * 	- we don't include ANIMFILTER_CURVEVISIBLE filter, as that will result in a 
		 * 	  mismatch between channel-colors and the drawn curves
		 */
		filter= (ANIMFILTER_VISIBLE|ANIMFILTER_CURVESONLY);
		items= ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
		
		/* loop over F-Curves, assigning colors */
		for (ale=anim_data.first, i=0; ale; ale= ale->next, i++) {
			FCurve *fcu= (FCurve *)ale->data;
			
			/* set color of curve here */
			switch (fcu->color_mode) {
				case FCURVE_COLOR_CUSTOM:
					/* User has defined a custom color for this curve already (we assume it's not going to cause clashes with text colors),
					 * which should be left alone... Nothing needs to be done here.
					 */
					break;
					
				case FCURVE_COLOR_AUTO_RGB:
				{
					/* F-Curve's array index is automatically mapped to RGB values. This works best of 3-value vectors. 
					 * TODO: find a way to module the hue so that not all curves have same color...
					 */
					
					/* standard table of colors to use */
					const float _colorsets[4][3]= 
					{
						{1.0f, 0.0f, 0.0f}, /* red */
						{0.0f, 1.0f, 0.0f}, /* green */
						{0.0f, 0.0f, 1.0f}, /* blue */
						{0.3f, 0.8f, 1.0f}, /* 'unknown' color - bluish so as to not conflict with handles */
					};
					
					/* simply copy the relevant color over to the F-Curve */
					if ((fcu->array_index >= 0) && (fcu->array_index < 3)) {
						/* if the index is within safe bounds, use index to access table */
						VECCOPY(fcu->color, _colorsets[fcu->array_index]);
					}
					else {
						/* use the 'unknown' color... */
						VECCOPY(fcu->color, _colorsets[3]);
					}
				}
					break;
				
				case FCURVE_COLOR_AUTO_RAINBOW:
				default:
				{
					/* determine color 'automatically' using 'magic function' which uses the given args
					 * of current item index + total items to determine some RGB color
					 */
					ipo_rainbow(i, items, fcu->color);
				}
					break;
			}
		}
		
		/* free temp list */
		BLI_freelistN(&anim_data);
	}
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_ipo(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype ipo");
	ARegionType *art;
	
	st->spaceid= SPACE_IPO;
	
	st->new= graph_new;
	st->free= graph_free;
	st->init= graph_init;
	st->duplicate= graph_duplicate;
	st->operatortypes= graphedit_operatortypes;
	st->keymap= graphedit_keymap;
	st->listener= graph_listener;
	st->refresh= graph_refresh;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype graphedit region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= graph_main_area_init;
	art->draw= graph_main_area_draw;
	art->listener= graph_region_listener;
	art->keymapflag= ED_KEYMAP_VIEW2D/*|ED_KEYMAP_MARKERS*/|ED_KEYMAP_ANIMATION|ED_KEYMAP_FRAMES;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype graphedit region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES;
	art->listener= graph_region_listener;
	art->init= graph_header_area_init;
	art->draw= graph_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: channels */
	art= MEM_callocN(sizeof(ARegionType), "spacetype graphedit region");
	art->regionid = RGN_TYPE_CHANNELS;
	art->minsizex= 200+V2D_SCROLL_WIDTH; /* 200 is the 'standard', but due to scrollers, we want a bit more to fit the lock icons in */
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES;
	art->listener= graph_region_listener;
	art->init= graph_channel_area_init;
	art->draw= graph_channel_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: UI buttons */
	art= MEM_callocN(sizeof(ARegionType), "spacetype graphedit region");
	art->regionid = RGN_TYPE_UI;
	art->minsizex= 200;
	art->keymapflag= ED_KEYMAP_UI;
	art->listener= graph_region_listener;
	art->init= graph_buttons_area_init;
	art->draw= graph_buttons_area_draw;
	
	BLI_addhead(&st->regiontypes, art);

	graph_buttons_register(art);
	
	BKE_spacetype_register(st);
}

