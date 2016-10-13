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

/** \file blender/editors/space_time/space_time.c
 *  \ingroup sptime
 */


#include <string.h>
#include <stdio.h>

#include "DNA_cachefile_types.h"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_dlrbTree.h"
#include "BLI_utildefines.h"

#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_screen.h"

#include "ED_anim_api.h"
#include "ED_keyframes_draw.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_resources.h"
#include "UI_view2d.h"
#include "UI_interface.h"

#include "ED_space_api.h"
#include "ED_markers.h"

#include "GPU_immediate.h"

#include "time_intern.h"

/* ************************ main time area region *********************** */

static void time_draw_sfra_efra(Scene *scene, View2D *v2d)
{	
	/* draw darkened area outside of active timeline 
	 * frame range used is preview range or scene range 
	 */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	VertexFormat *format = immVertexFormat();
	unsigned pos = add_attrib(format, "pos", GL_FLOAT, 2, KEEP_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
	immUniformColor4f(0.0f, 0.0f, 0.0f, 0.4f);

	if (PSFRA < PEFRA) {
		immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, (float)PSFRA, v2d->cur.ymax);
		immRectf(pos, (float)PEFRA, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
	}
	else {
		immRectf(pos, v2d->cur.xmin, v2d->cur.ymin, v2d->cur.xmax, v2d->cur.ymax);
	}

	glDisable(GL_BLEND);

	/* thin lines where the actual frames are */
	immUniformThemeColorShade(TH_BACK, -60);

	immBegin(GL_LINES, 4);

	immVertex2f(pos, (float)PSFRA, v2d->cur.ymin);
	immVertex2f(pos, (float)PSFRA, v2d->cur.ymax);

	immVertex2f(pos, (float)PEFRA, v2d->cur.ymin);
	immVertex2f(pos, (float)PEFRA, v2d->cur.ymax);

	immEnd();
	immUnbindProgram();
}

static void time_cache_free(SpaceTime *stime)
{
	SpaceTimeCache *stc;
	
	for (stc = stime->caches.first; stc; stc = stc->next) {
		if (stc->array) {
			MEM_freeN(stc->array);
			stc->array = NULL;
		}
	}
	
	BLI_freelistN(&stime->caches);
}

static void time_cache_refresh(SpaceTime *stime)
{
	/* Free previous caches to indicate full refresh */
	time_cache_free(stime);
}

/* helper function - find actkeycolumn that occurs on cframe, or the nearest one if not found */
static ActKeyColumn *time_cfra_find_ak(ActKeyColumn *ak, float cframe)
{
	ActKeyColumn *akn = NULL;
	
	/* sanity checks */
	if (ak == NULL)
		return NULL;
	
	/* check if this is a match, or whether it is in some subtree */
	if (cframe < ak->cfra)
		akn = time_cfra_find_ak(ak->left, cframe);
	else if (cframe > ak->cfra)
		akn = time_cfra_find_ak(ak->right, cframe);
		
	/* if no match found (or found match), just use the current one */
	if (akn == NULL)
		return ak;
	else
		return akn;
}

/* helper for time_draw_keyframes() */
static void time_draw_idblock_keyframes(View2D *v2d, ID *id, short onlysel, const unsigned char color[3])
{
	bDopeSheet ads = {NULL};
	DLRBT_Tree keys;
	ActKeyColumn *ak;
	
	float fac1 = (GS(id->name) == ID_GD) ? 0.8f : 0.6f; /* draw GPencil keys taller, to help distinguish them */
	float fac2 = 1.0f - fac1;
	
	float ymin = v2d->tot.ymin;
	float ymax = v2d->tot.ymax * fac1 + ymin * fac2;
	
	/* init binarytree-list for getting keyframes */
	BLI_dlrbTree_init(&keys);
	
	/* init dopesheet settings */
	if (onlysel)
		ads.filterflag |= ADS_FILTER_ONLYSEL;

	/* populate tree with keyframe nodes */
	switch (GS(id->name)) {
		case ID_SCE:
			scene_to_keylist(&ads, (Scene *)id, &keys, NULL);
			break;
		case ID_OB:
			ob_to_keylist(&ads, (Object *)id, &keys, NULL);
			break;
		case ID_GD:
			gpencil_to_keylist(&ads, (bGPdata *)id, &keys);
			break;
		case ID_CF:
			cachefile_to_keylist(&ads, (CacheFile *)id, &keys, NULL);
			break;
	}
		
	/* build linked-list for searching */
	BLI_dlrbTree_linkedlist_sync(&keys);
	
	/* start drawing keyframes 
	 *	- we use the binary-search capabilities of the tree to only start from 
	 *	  the first visible keyframe (last one can then be easily checked)
	 *	- draw within a single GL block to be faster
	 */

	ActKeyColumn *link;
	int max_len = 0;

	ak = time_cfra_find_ak(keys.root, v2d->cur.xmin);

	for (link = ak; link; link = link->next) {
		max_len++;
	}

	if (max_len > 0) {

		VertexFormat *format = immVertexFormat();
		unsigned pos = add_attrib(format, "pos", GL_FLOAT, 2, KEEP_FLOAT);

		immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
		immUniformColor3ubv(color);

		immBeginAtMost(GL_LINES, max_len * 2);

		for (; (ak) && (ak->cfra <= v2d->cur.xmax);
			ak = ak->next)
		{
			immVertex2f(pos, ak->cfra, ymin);
			immVertex2f(pos, ak->cfra, ymax);
		}

		immEnd();
		immUnbindProgram();
	}

	/* free temp stuff */
	BLI_dlrbTree_free(&keys);
}

static void time_draw_caches_keyframes(Main *bmain, Scene *scene, View2D *v2d, bool onlysel, const unsigned char color[3])
{
	CacheFile *cache_file;

	for (cache_file = bmain->cachefiles.first;
	     cache_file;
	     cache_file = cache_file->id.next)
	{
		cache_file->draw_flag &= ~CACHEFILE_KEYFRAME_DRAWN;
	}

	for (Base *base = scene->base.first; base; base = base->next) {
		Object *ob = base->object;

		ModifierData *md = modifiers_findByType(ob, eModifierType_MeshSequenceCache);

		if (md) {
			MeshSeqCacheModifierData *mcmd = (MeshSeqCacheModifierData *)md;

			cache_file = mcmd->cache_file;

			if (!cache_file || (cache_file->draw_flag & CACHEFILE_KEYFRAME_DRAWN) != 0) {
				continue;
			}

			cache_file->draw_flag |= CACHEFILE_KEYFRAME_DRAWN;

			time_draw_idblock_keyframes(v2d, (ID *)cache_file, onlysel, color);
		}

		for (bConstraint *con = ob->constraints.first; con; con = con->next) {
			if (con->type != CONSTRAINT_TYPE_TRANSFORM_CACHE) {
				continue;
			}

			bTransformCacheConstraint *data = con->data;

			cache_file = data->cache_file;

			if (!cache_file || (cache_file->draw_flag & CACHEFILE_KEYFRAME_DRAWN) != 0) {
				continue;
			}

			cache_file->draw_flag |= CACHEFILE_KEYFRAME_DRAWN;

			time_draw_idblock_keyframes(v2d, (ID *)cache_file, onlysel, color);
		}
	}
}

/* draw keyframe lines for timeline */
static void time_draw_keyframes(const bContext *C, ARegion *ar)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	View2D *v2d = &ar->v2d;
	bool onlysel = ((scene->flag & SCE_KEYS_NO_SELONLY) == 0);
	unsigned char color[3];
	
	/* set this for all keyframe lines once and for all */
	glLineWidth(1.0);

	/* draw cache files keyframes (if available) */
	UI_GetThemeColor3ubv(TH_TIME_KEYFRAME, color);
	time_draw_caches_keyframes(CTX_data_main(C), scene, v2d, onlysel, color);

	/* draw grease pencil keyframes (if available) */	
	UI_GetThemeColor3ubv(TH_TIME_GP_KEYFRAME, color);

	if (scene->gpd) {
		time_draw_idblock_keyframes(v2d, (ID *)scene->gpd, onlysel, color);
	}
	if (ob && ob->gpd) {
		time_draw_idblock_keyframes(v2d, (ID *)ob->gpd, onlysel, color);
	}
	
	/* draw scene keyframes first 
	 *	- don't try to do this when only drawing active/selected data keyframes,
	 *	  since this can become quite slow
	 */
	if (onlysel == 0) {
		/* set draw color */
		UI_GetThemeColorShade3ubv(TH_TIME_KEYFRAME, -50, color);
		time_draw_idblock_keyframes(v2d, (ID *)scene, onlysel, color);
	}
	
	/* draw keyframes from selected objects 
	 *  - only do the active object if in posemode (i.e. showing only keyframes for the bones)
	 *    OR the onlysel flag was set, which means that only active object's keyframes should
	 *    be considered
	 */
	UI_GetThemeColor3ubv(TH_TIME_KEYFRAME, color);

	if (ob && ((ob->mode == OB_MODE_POSE) || onlysel)) {
		/* draw keyframes for active object only */
		time_draw_idblock_keyframes(v2d, (ID *)ob, onlysel, color);
	}
	else {
		bool active_done = false;
		
		/* draw keyframes from all selected objects */
		CTX_DATA_BEGIN (C, Object *, obsel, selected_objects)
		{
			/* last arg is 0, since onlysel doesn't apply here... */
			time_draw_idblock_keyframes(v2d, (ID *)obsel, 0, color);
			
			/* if this object is the active one, set flag so that we don't draw again */
			if (obsel == ob)
				active_done = true;
		}
		CTX_DATA_END;
		
		/* if active object hasn't been done yet, draw it... */
		if (ob && (active_done == 0))
			time_draw_idblock_keyframes(v2d, (ID *)ob, 0, color);
	}
}

/* ---------------- */

static void time_refresh(const bContext *UNUSED(C), ScrArea *sa)
{
	/* find the main timeline region and refresh cache display*/
	ARegion *ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
	if (ar) {
		SpaceTime *stime = (SpaceTime *)sa->spacedata.first;
		time_cache_refresh(stime);
	}
}

/* editor level listener */
static void time_listener(bScreen *UNUSED(sc), ScrArea *sa, wmNotifier *wmn)
{

	/* mainly for updating cache display */
	switch (wmn->category) {
		case NC_OBJECT:
		{
			switch (wmn->data) {
				case ND_BONE_SELECT:
				case ND_BONE_ACTIVE:
				case ND_POINTCACHE:
				case ND_MODIFIER:
				case ND_KEYS:
					ED_area_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		}
		case NC_SCENE:
		{
			switch (wmn->data) {
				case ND_RENDER_RESULT:
					ED_area_tag_redraw(sa);
					break;
				case ND_OB_ACTIVE:
				case ND_FRAME:
					ED_area_tag_refresh(sa);
					break;
				case ND_FRAME_RANGE:
				{
					ARegion *ar;
					Scene *scene = wmn->reference;

					for (ar = sa->regionbase.first; ar; ar = ar->next) {
						if (ar->regiontype == RGN_TYPE_WINDOW) {
							ar->v2d.tot.xmin = (float)(SFRA - 4);
							ar->v2d.tot.xmax = (float)(EFRA + 4);
							break;
						}
					}
					break;
				}
			}
			break;
		}
		case NC_SPACE:
		{
			switch (wmn->data) {
				case ND_SPACE_CHANGED:
					ED_area_tag_refresh(sa);
					break;
			}
			break;
		}
		case NC_WM:
		{
			switch (wmn->data) {
				case ND_FILEREAD:
					ED_area_tag_refresh(sa);
					break;
			}
			break;
		}
	}
}

/* ---------------- */

/* add handlers, stuff you only do once or on area/region changes */
static void time_main_region_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	/* own keymap */
	keymap = WM_keymap_find(wm->defaultconf, "Timeline", SPACE_TIME, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void time_main_region_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	Scene *scene = CTX_data_scene(C);
	SpaceTime *stime = CTX_wm_space_time(C);
	View2D *v2d = &ar->v2d;
	View2DGrid *grid;
	View2DScrollers *scrollers;
	int unit, flag = 0;
	
	/* clear and setup matrix */
	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(v2d);
	
	/* grid */
	unit = (stime->flag & TIME_DRAWFRAMES) ? V2D_UNIT_FRAMES : V2D_UNIT_SECONDS;
	grid = UI_view2d_grid_calc(scene, v2d, unit, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY, ar->winx, ar->winy);
	UI_view2d_grid_draw(v2d, grid, (V2D_VERTICAL_LINES | V2D_VERTICAL_AXIS));
	UI_view2d_grid_free(grid);
	
	ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);

	/* start and end frame */
	time_draw_sfra_efra(scene, v2d);
	
	/* current frame */
	flag = DRAWCFRA_WIDE; /* this is only really needed on frames where there's a keyframe, but this will do... */
	if ((stime->flag & TIME_DRAWFRAMES) == 0)  flag |= DRAWCFRA_UNIT_SECONDS;
	if (stime->flag & TIME_CFRA_NUM)           flag |= DRAWCFRA_SHOW_NUMBOX;
	ANIM_draw_cfra(C, v2d, flag);
	
	UI_view2d_view_ortho(v2d);
	
	/* keyframes */
	time_draw_keyframes(C, ar);
	
	/* markers */
	UI_view2d_view_orthoSpecial(ar, v2d, 1);
	ED_markers_draw(C, 0);
	
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

static void time_main_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SPACE:
			if (wmn->data == ND_SPACE_TIME)
				ED_region_tag_redraw(ar);
			break;

		case NC_ANIMATION:
			ED_region_tag_redraw(ar);
			break;
		
		case NC_SCENE:
			switch (wmn->data) {
				case ND_OB_SELECT:
				case ND_OB_ACTIVE:
				case ND_FRAME:
				case ND_FRAME_RANGE:
				case ND_KEYINGSET:
				case ND_RENDER_OPTIONS:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_GPENCIL:
			if (wmn->data == ND_DATA)
				ED_region_tag_redraw(ar);
			break;
	}
}

/* ************************ header time area region *********************** */

/* add handlers, stuff you only do once or on area/region changes */
static void time_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void time_header_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void time_header_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCREEN:
		{
			if (wmn->data == ND_ANIMPLAY)
				ED_region_tag_redraw(ar);
			break;
		}
		case NC_SCENE:
		{
			switch (wmn->data) {
				case ND_RENDER_RESULT:
				case ND_OB_SELECT:
				case ND_FRAME:
				case ND_FRAME_RANGE:
				case ND_KEYINGSET:
				case ND_RENDER_OPTIONS:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		}
		case NC_SPACE:
		{
			if (wmn->data == ND_SPACE_TIME)
				ED_region_tag_redraw(ar);
			break;
		}
	}
}

/* ******************** default callbacks for time space ***************** */

static SpaceLink *time_new(const bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	ARegion *ar;
	SpaceTime *stime;

	stime = MEM_callocN(sizeof(SpaceTime), "inittime");

	stime->spacetype = SPACE_TIME;
	stime->flag |= TIME_DRAWFRAMES;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for time");
	
	BLI_addtail(&stime->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;
	
	/* main region */
	ar = MEM_callocN(sizeof(ARegion), "main region for time");
	
	BLI_addtail(&stime->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;
	
	ar->v2d.tot.xmin = (float)(SFRA - 4);
	ar->v2d.tot.ymin = 0.0f;
	ar->v2d.tot.xmax = (float)(EFRA + 4);
	ar->v2d.tot.ymax = 50.0f;
	
	ar->v2d.cur = ar->v2d.tot;

	ar->v2d.min[0] = 1.0f;
	ar->v2d.min[1] = 50.0f;

	ar->v2d.max[0] = MAXFRAMEF;
	ar->v2d.max[1] = 50.0;

	ar->v2d.minzoom = 0.1f;
	ar->v2d.maxzoom = 10.0;

	ar->v2d.scroll |= (V2D_SCROLL_BOTTOM | V2D_SCROLL_SCALE_HORIZONTAL);
	ar->v2d.align |= V2D_ALIGN_NO_NEG_Y;
	ar->v2d.keepofs |= V2D_LOCKOFS_Y;
	ar->v2d.keepzoom |= V2D_LOCKZOOM_Y;


	return (SpaceLink *)stime;
}

/* not spacelink itself */
static void time_free(SpaceLink *sl)
{
	SpaceTime *stime = (SpaceTime *)sl;
	
	time_cache_free(stime);
}
/* spacetype; init callback in ED_area_initialize() */
/* init is called to (re)initialize an existing editor (file read, screen changes) */
/* validate spacedata, add own area level handlers */
static void time_init(wmWindowManager *UNUSED(wm), ScrArea *sa)
{
	SpaceTime *stime = (SpaceTime *)sa->spacedata.first;
	
	time_cache_free(stime);
	
	/* enable all cache display */
	stime->cache_display |= TIME_CACHE_DISPLAY;
	stime->cache_display |= (TIME_CACHE_SOFTBODY | TIME_CACHE_PARTICLES);
	stime->cache_display |= (TIME_CACHE_CLOTH | TIME_CACHE_SMOKE | TIME_CACHE_DYNAMICPAINT);
	stime->cache_display |= TIME_CACHE_RIGIDBODY;
}

static SpaceLink *time_duplicate(SpaceLink *sl)
{
	SpaceTime *stime = (SpaceTime *)sl;
	SpaceTime *stimen = MEM_dupallocN(stime);
	
	BLI_listbase_clear(&stimen->caches);
	
	return (SpaceLink *)stimen;
}

/* only called once, from space_api/spacetypes.c */
/* it defines all callbacks to maintain spaces */
void ED_spacetype_time(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype time");
	ARegionType *art;
	
	st->spaceid = SPACE_TIME;
	strncpy(st->name, "Timeline", BKE_ST_MAXNAME);
	
	st->new = time_new;
	st->free = time_free;
	st->init = time_init;
	st->duplicate = time_duplicate;
	st->operatortypes = time_operatortypes;
	st->keymap = NULL;
	st->listener = time_listener;
	st->refresh = time_refresh;
	
	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype time region");
	art->regionid = RGN_TYPE_WINDOW;
	art->keymapflag = ED_KEYMAP_VIEW2D | ED_KEYMAP_MARKERS | ED_KEYMAP_ANIMATION | ED_KEYMAP_FRAMES;
	
	art->init = time_main_region_init;
	art->draw = time_main_region_draw;
	art->listener = time_main_region_listener;
	art->keymap = time_keymap;
	art->lock = 1;   /* Due to pointcache, see T4960. */
	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype time region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
	
	art->init = time_header_region_init;
	art->draw = time_header_region_draw;
	art->listener = time_header_region_listener;
	BLI_addhead(&st->regiontypes, art);
		
	BKE_spacetype_register(st);
}
