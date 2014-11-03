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

/** \file blender/editors/space_graph/space_graph.c
 *  \ingroup spgraph
 */


#include <string.h>
#include <stdio.h>

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_fcurve.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_anim_api.h"
#include "ED_markers.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "graph_intern.h"   // own include

/* ******************** manage regions ********************* */

ARegion *graph_has_buttons_region(ScrArea *sa)
{
	ARegion *ar, *arnew;
	
	ar = BKE_area_find_region_type(sa, RGN_TYPE_UI);
	if (ar) return ar;

	/* add subdiv level; after main */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);

	/* is error! */
	if (ar == NULL) return NULL;
	
	arnew = MEM_callocN(sizeof(ARegion), "buttons for graph");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype = RGN_TYPE_UI;
	arnew->alignment = RGN_ALIGN_RIGHT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}


/* ******************** default callbacks for ipo space ***************** */

static SpaceLink *graph_new(const bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	ARegion *ar;
	SpaceIpo *sipo;
	
	/* Graph Editor - general stuff */
	sipo = MEM_callocN(sizeof(SpaceIpo), "init graphedit");
	sipo->spacetype = SPACE_IPO;
	
	sipo->autosnap = SACTSNAP_FRAME;
	
	/* allocate DopeSheet data for Graph Editor */
	sipo->ads = MEM_callocN(sizeof(bDopeSheet), "GraphEdit DopeSheet");
	sipo->ads->source = (ID *)scene;
	
	/* settings for making it easier by default to just see what you're interested in tweaking */
	sipo->ads->filterflag |= ADS_FILTER_ONLYSEL;
	sipo->flag |= SIPO_SELVHANDLESONLY;
	
	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for graphedit");
	
	BLI_addtail(&sipo->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;
	
	/* channels */
	ar = MEM_callocN(sizeof(ARegion), "channels area for graphedit");
	
	BLI_addtail(&sipo->regionbase, ar);
	ar->regiontype = RGN_TYPE_CHANNELS;
	ar->alignment = RGN_ALIGN_LEFT;
	
	ar->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
	
	/* ui buttons */
	ar = MEM_callocN(sizeof(ARegion), "buttons area for graphedit");
	
	BLI_addtail(&sipo->regionbase, ar);
	ar->regiontype = RGN_TYPE_UI;
	ar->alignment = RGN_ALIGN_RIGHT;
	ar->flag = RGN_FLAG_HIDDEN;
	
	/* main area */
	ar = MEM_callocN(sizeof(ARegion), "main area for graphedit");
	
	BLI_addtail(&sipo->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;
	
	ar->v2d.tot.xmin = 0.0f;
	ar->v2d.tot.ymin = (float)scene->r.sfra - 10.0f;
	ar->v2d.tot.xmax = (float)scene->r.efra;
	ar->v2d.tot.ymax = 10.0f;
	
	ar->v2d.cur = ar->v2d.tot;
	
	ar->v2d.min[0] = FLT_MIN;
	ar->v2d.min[1] = FLT_MIN;

	ar->v2d.max[0] = MAXFRAMEF;
	ar->v2d.max[1] = FLT_MAX;
	
	ar->v2d.scroll = (V2D_SCROLL_BOTTOM | V2D_SCROLL_SCALE_HORIZONTAL);
	ar->v2d.scroll |= (V2D_SCROLL_LEFT | V2D_SCROLL_SCALE_VERTICAL);
	
	ar->v2d.keeptot = 0;
	
	return (SpaceLink *)sipo;
}

/* not spacelink itself */
static void graph_free(SpaceLink *sl)
{	
	SpaceIpo *si = (SpaceIpo *)sl;
	
	if (si->ads) {
		BLI_freelistN(&si->ads->chanbase);
		MEM_freeN(si->ads);
	}
	
	if (si->ghostCurves.first)
		free_fcurves(&si->ghostCurves);
}


/* spacetype; init callback */
static void graph_init(struct wmWindowManager *UNUSED(wm), ScrArea *sa)
{
	SpaceIpo *sipo = (SpaceIpo *)sa->spacedata.first;
	
	/* init dopesheet data if non-existent (i.e. for old files) */
	if (sipo->ads == NULL) {
		sipo->ads = MEM_callocN(sizeof(bDopeSheet), "GraphEdit DopeSheet");
		sipo->ads->source = (ID *)(G.main->scene.first); // FIXME: this is a really nasty hack here for now...
	}
	
	/* force immediate init of any invalid F-Curve colors */
	/* XXX: but, don't do SIPO_TEMP_NEEDCHANSYNC (i.e. channel select state sync)
	 * as this is run on each region resize; setting this here will cause selection
	 * state to be lost on area/region resizing. [#35744]
	 */
	ED_area_tag_refresh(sa);
}

static SpaceLink *graph_duplicate(SpaceLink *sl)
{
	SpaceIpo *sipon = MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	BLI_duplicatelist(&sipon->ghostCurves, &((SpaceIpo *)sl)->ghostCurves);
	sipon->ads = MEM_dupallocN(sipon->ads);
	
	return (SpaceLink *)sipon;
}

/* add handlers, stuff you only do once or on area/region changes */
static void graph_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	/* own keymap */
	keymap = WM_keymap_find(wm->defaultconf, "Graph Editor", SPACE_IPO, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
	keymap = WM_keymap_find(wm->defaultconf, "Graph Editor Generic", SPACE_IPO, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void graph_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceIpo *sipo = CTX_wm_space_graph(C);
	bAnimContext ac;
	View2D *v2d = &ar->v2d;
	View2DGrid *grid;
	View2DScrollers *scrollers;
	float col[3];
	short unitx = 0, unity = V2D_UNIT_VALUES, flag = 0;
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(v2d);
	
	/* grid */
	unitx = (sipo->flag & SIPO_DRAWTIME) ? V2D_UNIT_SECONDS : V2D_UNIT_FRAMESCALE;
	grid = UI_view2d_grid_calc(CTX_data_scene(C), v2d, unitx, V2D_GRID_NOCLAMP, unity, V2D_GRID_NOCLAMP, ar->winx, ar->winy);
	UI_view2d_grid_draw(v2d, grid, V2D_GRIDLINES_ALL);
	
	ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);

	/* draw data */
	if (ANIM_animdata_get_context(C, &ac)) {
		/* draw ghost curves */
		graph_draw_ghost_curves(&ac, sipo, ar);
		
		/* draw curves twice - unselected, then selected, so that the are fewer occlusion problems */
		graph_draw_curves(&ac, sipo, ar, grid, 0);
		graph_draw_curves(&ac, sipo, ar, grid, 1);
		
		/* XXX the slow way to set tot rect... but for nice sliders needed (ton) */
		get_graph_keyframe_extents(&ac, &v2d->tot.xmin, &v2d->tot.xmax, &v2d->tot.ymin, &v2d->tot.ymax, false, true);
		/* extra offset so that these items are visible */
		v2d->tot.xmin -= 10.0f;
		v2d->tot.xmax += 10.0f;
	}
	
	/* only free grid after drawing data, as we need to use it to determine sampling rate */
	UI_view2d_grid_free(grid);
	
	/* horizontal component of value-cursor (value line before the current frame line) */
	if ((sipo->flag & SIPO_NODRAWCURSOR) == 0) {
		float vec[2];
		
		/* Draw a green line to indicate the cursor value */
		vec[1] = sipo->cursorVal;
		
		UI_ThemeColorShadeAlpha(TH_CFRAME, -10, -50);
		glLineWidth(2.0);
		
		glEnable(GL_BLEND);
		glBegin(GL_LINE_STRIP);
		vec[0] = v2d->cur.xmin;
		glVertex2fv(vec);
			
		vec[0] = v2d->cur.xmax;
		glVertex2fv(vec);
		glEnd(); // GL_LINE_STRIP
		glDisable(GL_BLEND);
	}
	
	/* current frame */
	if (sipo->flag & SIPO_DRAWTIME) flag |= DRAWCFRA_UNIT_SECONDS;
	if ((sipo->flag & SIPO_NODRAWCFRANUM) == 0) flag |= DRAWCFRA_SHOW_NUMBOX;
	ANIM_draw_cfra(C, v2d, flag);
	
	/* markers */
	UI_view2d_view_orthoSpecial(ar, v2d, 1);
	ED_markers_draw(C, DRAW_MARKERS_MARGIN);
	
	/* preview range */
	UI_view2d_view_ortho(v2d);
	ANIM_draw_previewrange(C, v2d, 0);
	
	/* callback */
	UI_view2d_view_ortho(v2d);
	ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);

	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	// FIXME: args for scrollers depend on the type of data being shown...
	scrollers = UI_view2d_scrollers_calc(C, v2d, unitx, V2D_GRID_NOCLAMP, unity, V2D_GRID_NOCLAMP);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

static void graph_channel_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	/* make sure we keep the hide flags */
	ar->v2d.scroll |= (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
	ar->v2d.scroll &= ~(V2D_SCROLL_LEFT | V2D_SCROLL_TOP);	/* prevent any noise of past */
	ar->v2d.scroll |= V2D_SCROLL_HORIZONTAL_HIDE;
	ar->v2d.scroll |= V2D_SCROLL_VERTICAL_HIDE;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);
	
	/* own keymap */
	keymap = WM_keymap_find(wm->defaultconf, "Animation Channels", 0, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
	keymap = WM_keymap_find(wm->defaultconf, "Graph Editor Generic", SPACE_IPO, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void graph_channel_area_draw(const bContext *C, ARegion *ar)
{
	bAnimContext ac;
	View2D *v2d = &ar->v2d;
	View2DScrollers *scrollers;
	float col[3];
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(v2d);
	
	/* draw channels */
	if (ANIM_animdata_get_context(C, &ac)) {
		graph_draw_channel_names((bContext *)C, &ac, ar);
	}
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers = UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

/* add handlers, stuff you only do once or on area/region changes */
static void graph_header_area_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void graph_header_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

/* add handlers, stuff you only do once or on area/region changes */
static void graph_buttons_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	ED_region_panels_init(wm, ar);

	keymap = WM_keymap_find(wm->defaultconf, "Graph Editor Generic", SPACE_IPO, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void graph_buttons_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, NULL, -1);
}

static void graph_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
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
				case ND_SEQUENCER:
					if (wmn->action == NA_SELECTED)
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
		case NC_NODE:
			switch (wmn->action) {
				case NA_EDITED:
				case NA_SELECTED:
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
static void graph_listener(bScreen *UNUSED(sc), ScrArea *sa, wmNotifier *wmn)
{
	SpaceIpo *sipo = (SpaceIpo *)sa->spacedata.first;
	
	/* context changes */
	switch (wmn->category) {
		case NC_ANIMATION:
			/* for selection changes of animation data, we can just redraw... otherwise autocolor might need to be done again */
			if (ELEM(wmn->data, ND_KEYFRAME, ND_ANIMCHAN) && (wmn->action == NA_SELECTED))
				ED_area_tag_redraw(sa);
			else
				ED_area_tag_refresh(sa);
			break;
		case NC_SCENE:
			switch (wmn->data) {
				case ND_OB_ACTIVE:  /* selection changed, so force refresh to flush (needs flag set to do syncing)  */
				case ND_OB_SELECT:
					sipo->flag |= SIPO_TEMP_NEEDCHANSYNC;
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
					sipo->flag |= SIPO_TEMP_NEEDCHANSYNC;
					ED_area_tag_refresh(sa);
					break;
				case ND_TRANSFORM:
					break; /*do nothing*/
					
				default: /* just redrawing the view will do */
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_NODE:
			if (wmn->action == NA_SELECTED) {
				/* selection changed, so force refresh to flush (needs flag set to do syncing) */
				sipo->flag |= SIPO_TEMP_NEEDCHANSYNC;
				ED_area_tag_refresh(sa);
			}
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_GRAPH)
				ED_area_tag_redraw(sa);
			break;
		case NC_WINDOW:
			if (sipo->flag & SIPO_TEMP_NEEDCHANSYNC) {
				/* force redraw/refresh after undo/redo - prevents "black curve" problem */
				ED_area_tag_refresh(sa);
			}
			break;
			
			// XXX: restore the case below if not enough updates occur...
			//default:
			//	if (wmn->data == ND_KEYS)
			//		ED_area_tag_redraw(sa);
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
			break;
		}
		
		case SIPO_MODE_DRIVERS: /* drivers only  */
		{
			break;
		}
	}
	
	/* region updates? */
	// XXX re-sizing y-extents of tot should go here?
	
	/* update the state of the animchannels in response to changes from the data they represent 
	 * NOTE: the temp flag is used to indicate when this needs to be done, and will be cleared once handled
	 */
	if (sipo->flag & SIPO_TEMP_NEEDCHANSYNC) {
		ANIM_sync_animchannels_to_data(C);
		sipo->flag &= ~SIPO_TEMP_NEEDCHANSYNC;
		ED_area_tag_redraw(sa);
	}
	
	/* init/adjust F-Curve colors */
	if (ANIM_animdata_get_context(C, &ac)) {
		ListBase anim_data = {NULL, NULL};
		bAnimListElem *ale;
		size_t items;
		int filter;
		int i;
		
		UI_SetTheme(SPACE_IPO, RGN_TYPE_WINDOW);

		/* build list of F-Curves which will be visible as channels in channel-region
		 *  - we don't include ANIMFILTER_CURVEVISIBLE filter, as that will result in a
		 *    mismatch between channel-colors and the drawn curves
		 */
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS);
		items = ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
		
		/* loop over F-Curves, assigning colors */
		for (ale = anim_data.first, i = 0; ale; ale = ale->next, i++) {
			FCurve *fcu = (FCurve *)ale->data;
			
			/* set color of curve here */
			switch (fcu->color_mode) {
				case FCURVE_COLOR_CUSTOM:
				{
					/* User has defined a custom color for this curve already (we assume it's not going to cause clashes with text colors),
					 * which should be left alone... Nothing needs to be done here.
					 */
					break;
				}
				case FCURVE_COLOR_AUTO_RGB:
				{
					/* F-Curve's array index is automatically mapped to RGB values. This works best of 3-value vectors. 
					 * TODO: find a way to module the hue so that not all curves have same color...
					 */
					float *col = fcu->color;
					
					switch (fcu->array_index) {
						case 0:
							UI_GetThemeColor3fv(TH_AXIS_X, col);
							break;
						case 1:
							UI_GetThemeColor3fv(TH_AXIS_Y, col);
							break;
						case 2:
							UI_GetThemeColor3fv(TH_AXIS_Z, col);
							break;
						default:
							/* 'unknown' color - bluish so as to not conflict with handles */
							col[0] = 0.3f; col[1] = 0.8f; col[2] = 1.0f;
							break;
					}
					break;
				}
				case FCURVE_COLOR_AUTO_RAINBOW:
				default:
				{
					/* determine color 'automatically' using 'magic function' which uses the given args
					 * of current item index + total items to determine some RGB color
					 */
					getcolor_fcurve_rainbow(i, items, fcu->color);
					break;
				}
			}
		}
		
		/* free temp list */
		ANIM_animdata_freelist(&anim_data);
	}
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_ipo(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype ipo");
	ARegionType *art;
	
	st->spaceid = SPACE_IPO;
	strncpy(st->name, "Graph", BKE_ST_MAXNAME);
	
	st->new = graph_new;
	st->free = graph_free;
	st->init = graph_init;
	st->duplicate = graph_duplicate;
	st->operatortypes = graphedit_operatortypes;
	st->keymap = graphedit_keymap;
	st->listener = graph_listener;
	st->refresh = graph_refresh;
	
	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype graphedit region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init = graph_main_area_init;
	art->draw = graph_main_area_draw;
	art->listener = graph_region_listener;
	art->keymapflag = ED_KEYMAP_VIEW2D | ED_KEYMAP_MARKERS | ED_KEYMAP_ANIMATION | ED_KEYMAP_FRAMES;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype graphedit region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
	art->listener = graph_region_listener;
	art->init = graph_header_area_init;
	art->draw = graph_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: channels */
	art = MEM_callocN(sizeof(ARegionType), "spacetype graphedit region");
	art->regionid = RGN_TYPE_CHANNELS;
	art->prefsizex = 200 + V2D_SCROLL_WIDTH; /* 200 is the 'standard', but due to scrollers, we want a bit more to fit the lock icons in */
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES;
	art->listener = graph_region_listener;
	art->init = graph_channel_area_init;
	art->draw = graph_channel_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: UI buttons */
	art = MEM_callocN(sizeof(ARegionType), "spacetype graphedit region");
	art->regionid = RGN_TYPE_UI;
	art->prefsizex = 200;
	art->keymapflag = ED_KEYMAP_UI;
	art->listener = graph_region_listener;
	art->init = graph_buttons_area_init;
	art->draw = graph_buttons_area_draw;
	
	BLI_addhead(&st->regiontypes, art);

	graph_buttons_register(art);
	
	BKE_spacetype_register(st);
}

