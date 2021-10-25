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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/screen.c
 *  \ingroup bke
 */

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "GPU_compositing.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_rect.h"

#include "BKE_idprop.h"
#include "BKE_screen.h"

/* ************ Spacetype/regiontype handling ************** */

/* keep global; this has to be accessible outside of windowmanager */
static ListBase spacetypes = {NULL, NULL};

/* not SpaceType itself */
static void spacetype_free(SpaceType *st)
{
	ARegionType *art;
	PanelType *pt;
	HeaderType *ht;
	
	for (art = st->regiontypes.first; art; art = art->next) {
		BLI_freelistN(&art->drawcalls);

		for (pt = art->paneltypes.first; pt; pt = pt->next)
			if (pt->ext.free)
				pt->ext.free(pt->ext.data);

		for (ht = art->headertypes.first; ht; ht = ht->next)
			if (ht->ext.free)
				ht->ext.free(ht->ext.data);

		BLI_freelistN(&art->paneltypes);
		BLI_freelistN(&art->headertypes);
	}
	
	BLI_freelistN(&st->regiontypes);
	BLI_freelistN(&st->toolshelf);

}

void BKE_spacetypes_free(void)
{
	SpaceType *st;
	
	for (st = spacetypes.first; st; st = st->next) {
		spacetype_free(st);
	}
	
	BLI_freelistN(&spacetypes);
}

SpaceType *BKE_spacetype_from_id(int spaceid)
{
	SpaceType *st;
	
	for (st = spacetypes.first; st; st = st->next) {
		if (st->spaceid == spaceid)
			return st;
	}
	return NULL;
}

ARegionType *BKE_regiontype_from_id(SpaceType *st, int regionid)
{
	ARegionType *art;
	
	for (art = st->regiontypes.first; art; art = art->next)
		if (art->regionid == regionid)
			return art;
	
	printf("Error, region type %d missing in - name:\"%s\", id:%d\n", regionid, st->name, st->spaceid);
	return st->regiontypes.first;
}


const ListBase *BKE_spacetypes_list(void)
{
	return &spacetypes;
}

void BKE_spacetype_register(SpaceType *st)
{
	SpaceType *stype;
	
	/* sanity check */
	stype = BKE_spacetype_from_id(st->spaceid);
	if (stype) {
		printf("error: redefinition of spacetype %s\n", stype->name);
		spacetype_free(stype);
		MEM_freeN(stype);
	}
	
	BLI_addtail(&spacetypes, st);
}

bool BKE_spacetype_exists(int spaceid)
{
	return BKE_spacetype_from_id(spaceid) != NULL;
}

/* ***************** Space handling ********************** */

void BKE_spacedata_freelist(ListBase *lb)
{
	SpaceLink *sl;
	ARegion *ar;
	
	for (sl = lb->first; sl; sl = sl->next) {
		SpaceType *st = BKE_spacetype_from_id(sl->spacetype);
		
		/* free regions for pushed spaces */
		for (ar = sl->regionbase.first; ar; ar = ar->next)
			BKE_area_region_free(st, ar);

		BLI_freelistN(&sl->regionbase);
		
		if (st && st->free) 
			st->free(sl);
	}
	
	BLI_freelistN(lb);
}

ARegion *BKE_area_region_copy(SpaceType *st, ARegion *ar)
{
	ARegion *newar = MEM_dupallocN(ar);
	Panel *pa, *newpa, *patab;
	
	newar->prev = newar->next = NULL;
	BLI_listbase_clear(&newar->handlers);
	BLI_listbase_clear(&newar->uiblocks);
	BLI_listbase_clear(&newar->panels_category);
	BLI_listbase_clear(&newar->panels_category_active);
	BLI_listbase_clear(&newar->ui_lists);
	newar->swinid = 0;
	newar->regiontimer = NULL;
	newar->headerstr = NULL;
	
	/* use optional regiondata callback */
	if (ar->regiondata) {
		ARegionType *art = BKE_regiontype_from_id(st, ar->regiontype);

		if (art && art->duplicate)
			newar->regiondata = art->duplicate(ar->regiondata);
		else
			newar->regiondata = MEM_dupallocN(ar->regiondata);
	}

	if (ar->v2d.tab_offset)
		newar->v2d.tab_offset = MEM_dupallocN(ar->v2d.tab_offset);
	
	BLI_listbase_clear(&newar->panels);
	BLI_duplicatelist(&newar->panels, &ar->panels);

	BLI_listbase_clear(&newar->ui_previews);
	BLI_duplicatelist(&newar->ui_previews, &ar->ui_previews);

	/* copy panel pointers */
	for (newpa = newar->panels.first; newpa; newpa = newpa->next) {
		patab = newar->panels.first;
		pa = ar->panels.first;
		while (patab) {
			if (newpa->paneltab == pa) {
				newpa->paneltab = patab;
				break;
			}
			patab = patab->next;
			pa = pa->next;
		}
	}
	
	return newar;
}


/* from lb2 to lb1, lb1 is supposed to be freed */
static void region_copylist(SpaceType *st, ListBase *lb1, ListBase *lb2)
{
	ARegion *ar;
	
	/* to be sure */
	BLI_listbase_clear(lb1);
	
	for (ar = lb2->first; ar; ar = ar->next) {
		ARegion *arnew = BKE_area_region_copy(st, ar);
		BLI_addtail(lb1, arnew);
	}
}


/* lb1 should be empty */
void BKE_spacedata_copylist(ListBase *lb1, ListBase *lb2)
{
	SpaceLink *sl;
	
	BLI_listbase_clear(lb1);  /* to be sure */
	
	for (sl = lb2->first; sl; sl = sl->next) {
		SpaceType *st = BKE_spacetype_from_id(sl->spacetype);
		
		if (st && st->duplicate) {
			SpaceLink *slnew = st->duplicate(sl);
			
			BLI_addtail(lb1, slnew);
			
			region_copylist(st, &slnew->regionbase, &sl->regionbase);
		}
	}
}

/* facility to set locks for drawing to survive (render) threads accessing drawing data */
/* lock can become bitflag too */
/* should be replaced in future by better local data handling for threads */
void BKE_spacedata_draw_locks(int set)
{
	SpaceType *st;
	
	for (st = spacetypes.first; st; st = st->next) {
		ARegionType *art;
	
		for (art = st->regiontypes.first; art; art = art->next) {
			if (set) 
				art->do_lock = art->lock;
			else 
				art->do_lock = false;
		}
	}
}

static void (*spacedata_id_remap_cb)(struct ScrArea *sa, struct SpaceLink *sl, ID *old_id, ID *new_id) = NULL;

void BKE_spacedata_callback_id_remap_set(void (*func)(ScrArea *sa, SpaceLink *sl, ID *, ID *))
{
	spacedata_id_remap_cb = func;
}

/* UNUSED!!! */
void BKE_spacedata_id_unref(struct ScrArea *sa, struct SpaceLink *sl, struct ID *id)
{
	if (spacedata_id_remap_cb) {
		spacedata_id_remap_cb(sa, sl, id, NULL);
	}
}

/* not region itself */
void BKE_area_region_free(SpaceType *st, ARegion *ar)
{
	uiList *uilst;

	if (st) {
		ARegionType *art = BKE_regiontype_from_id(st, ar->regiontype);
		
		if (art && art->free)
			art->free(ar);
		
		if (ar->regiondata)
			printf("regiondata free error\n");
	}
	else if (ar->type && ar->type->free)
		ar->type->free(ar);
	
	if (ar->v2d.tab_offset) {
		MEM_freeN(ar->v2d.tab_offset);
		ar->v2d.tab_offset = NULL;
	}

	if (!BLI_listbase_is_empty(&ar->panels)) {
		Panel *pa, *pa_next;
		for (pa = ar->panels.first; pa; pa = pa_next) {
			pa_next = pa->next;
			if (pa->activedata) {
				MEM_freeN(pa->activedata);
			}
			MEM_freeN(pa);
		}
	}

	for (uilst = ar->ui_lists.first; uilst; uilst = uilst->next) {
		if (uilst->dyn_data) {
			uiListDyn *dyn_data = uilst->dyn_data;
			if (dyn_data->items_filter_flags) {
				MEM_freeN(dyn_data->items_filter_flags);
			}
			if (dyn_data->items_filter_neworder) {
				MEM_freeN(dyn_data->items_filter_neworder);
			}
			MEM_freeN(dyn_data);
		}
		if (uilst->properties) {
			IDP_FreeProperty(uilst->properties);
			MEM_freeN(uilst->properties);
		}
	}
	BLI_freelistN(&ar->ui_lists);
	BLI_freelistN(&ar->ui_previews);
	BLI_freelistN(&ar->panels_category);
	BLI_freelistN(&ar->panels_category_active);
}

/* not area itself */
void BKE_screen_area_free(ScrArea *sa)
{
	SpaceType *st = BKE_spacetype_from_id(sa->spacetype);
	ARegion *ar;
	
	for (ar = sa->regionbase.first; ar; ar = ar->next)
		BKE_area_region_free(st, ar);

	BLI_freelistN(&sa->regionbase);
	
	BKE_spacedata_freelist(&sa->spacedata);
	
	BLI_freelistN(&sa->actionzones);
}

/** Free (or release) any data used by this screen (does not free the screen itself). */
void BKE_screen_free(bScreen *sc)
{
	ScrArea *sa, *san;
	ARegion *ar;

	/* No animdata here. */
	
	for (ar = sc->regionbase.first; ar; ar = ar->next)
		BKE_area_region_free(NULL, ar);

	BLI_freelistN(&sc->regionbase);
	
	for (sa = sc->areabase.first; sa; sa = san) {
		san = sa->next;
		BKE_screen_area_free(sa);
	}
	
	BLI_freelistN(&sc->vertbase);
	BLI_freelistN(&sc->edgebase);
	BLI_freelistN(&sc->areabase);
}

/* for depsgraph */
unsigned int BKE_screen_visible_layers(bScreen *screen, Scene *scene)
{
	ScrArea *sa;
	unsigned int layer = 0;

	if (screen) {
		/* get all used view3d layers */
		for (sa = screen->areabase.first; sa; sa = sa->next)
			if (sa->spacetype == SPACE_VIEW3D)
				layer |= ((View3D *)sa->spacedata.first)->lay;
	}

	if (!layer)
		return scene->lay;

	return layer;
}

/* ***************** Utilities ********************** */

/* Find a region of the specified type from the given area */
ARegion *BKE_area_find_region_type(ScrArea *sa, int type)
{
	if (sa) {
		ARegion *ar;
		
		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if (ar->regiontype == type)
				return ar;
		}
	}
	return NULL;
}

ARegion *BKE_area_find_region_active_win(ScrArea *sa)
{
	if (sa) {
		ARegion *ar = BLI_findlink(&sa->regionbase, sa->region_active_win);
		if (ar && (ar->regiontype == RGN_TYPE_WINDOW)) {
			return ar;
		}

		/* fallback to any */
		return BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
	}
	return NULL;
}

ARegion *BKE_area_find_region_xy(ScrArea *sa, const int regiontype, int x, int y)
{
	ARegion *ar_found = NULL;
	if (sa) {
		ARegion *ar;
		for (ar = sa->regionbase.first; ar; ar = ar->next) {
			if ((regiontype == RGN_TYPE_ANY) || (ar->regiontype == regiontype)) {
				if (BLI_rcti_isect_pt(&ar->winrct, x, y)) {
					ar_found = ar;
					break;
				}
			}
		}
	}
	return ar_found;
}

/**
 * \note, ideally we can get the area from the context,
 * there are a few places however where this isn't practical.
 */
ScrArea *BKE_screen_find_area_from_space(struct bScreen *sc, SpaceLink *sl)
{
	ScrArea *sa;

	for (sa = sc->areabase.first; sa; sa = sa->next) {
		if (BLI_findindex(&sa->spacedata, sl) != -1) {
			break;
		}
	}

	return sa;
}

/**
 * \note Using this function is generally a last resort, you really want to be
 * using the context when you can - campbell
 */
ScrArea *BKE_screen_find_big_area(bScreen *sc, const int spacetype, const short min)
{
	ScrArea *sa, *big = NULL;
	int size, maxsize = 0;

	for (sa = sc->areabase.first; sa; sa = sa->next) {
		if ((spacetype == SPACE_TYPE_ANY) || (sa->spacetype == spacetype)) {
			if (min <= sa->winx && min <= sa->winy) {
				size = sa->winx * sa->winy;
				if (size > maxsize) {
					maxsize = size;
					big = sa;
				}
			}
		}
	}

	return big;
}

ScrArea *BKE_screen_find_area_xy(bScreen *sc, const int spacetype, int x, int y)
{
	ScrArea *sa, *sa_found = NULL;

	for (sa = sc->areabase.first; sa; sa = sa->next) {
		if (BLI_rcti_isect_pt(&sa->totrct, x, y)) {
			if ((spacetype == SPACE_TYPE_ANY) || (sa->spacetype == spacetype)) {
				sa_found = sa;
			}
			break;
		}
	}
	return sa_found;
}


/**
 * Utility function to get the active layer to use when adding new objects.
 */
unsigned int BKE_screen_view3d_layer_active_ex(const View3D *v3d, const Scene *scene, bool use_localvd)
{
	unsigned int lay;
	if ((v3d == NULL) || (v3d->scenelock && !v3d->localvd)) {
		lay = scene->layact;
	}
	else {
		lay = v3d->layact;
	}

	if (use_localvd) {
		if (v3d && v3d->localvd) {
			lay |= v3d->lay;
		}
	}

	return lay;
}
unsigned int BKE_screen_view3d_layer_active(const struct View3D *v3d, const struct Scene *scene)
{
	return BKE_screen_view3d_layer_active_ex(v3d, scene, true);
}

/**
 * Accumulate all visible layers on this screen.
 */
unsigned int BKE_screen_view3d_layer_all(const bScreen *sc)
{
	const ScrArea *sa;
	unsigned int lay = 0;
	for (sa = sc->areabase.first; sa; sa = sa->next) {
		if (sa->spacetype == SPACE_VIEW3D) {
			View3D *v3d = sa->spacedata.first;
			lay |= v3d->lay;
		}
	}

	return lay;
}

void BKE_screen_view3d_sync(View3D *v3d, struct Scene *scene)
{
	int bit;

	if (v3d->scenelock && v3d->localvd == NULL) {
		v3d->lay = scene->lay;
		v3d->camera = scene->camera;

		if (v3d->camera == NULL) {
			ARegion *ar;

			for (ar = v3d->regionbase.first; ar; ar = ar->next) {
				if (ar->regiontype == RGN_TYPE_WINDOW) {
					RegionView3D *rv3d = ar->regiondata;
					if (rv3d->persp == RV3D_CAMOB)
						rv3d->persp = RV3D_PERSP;
				}
			}
		}

		if ((v3d->lay & v3d->layact) == 0) {
			for (bit = 0; bit < 32; bit++) {
				if (v3d->lay & (1u << bit)) {
					v3d->layact = (1u << bit);
					break;
				}
			}
		}
	}
}

void BKE_screen_view3d_scene_sync(bScreen *sc)
{
	/* are there cameras in the views that are not in the scene? */
	ScrArea *sa;
	for (sa = sc->areabase.first; sa; sa = sa->next) {
		SpaceLink *sl;
		for (sl = sa->spacedata.first; sl; sl = sl->next) {
			if (sl->spacetype == SPACE_VIEW3D) {
				View3D *v3d = (View3D *) sl;
				BKE_screen_view3d_sync(v3d, sc->scene);
			}
		}
	}
}

void BKE_screen_view3d_main_sync(ListBase *screen_lb, Scene *scene)
{
	bScreen *sc;
	ScrArea *sa;
	SpaceLink *sl;

	/* from scene copy to the other views */
	for (sc = screen_lb->first; sc; sc = sc->id.next) {
		if (sc->scene != scene)
			continue;

		for (sa = sc->areabase.first; sa; sa = sa->next)
			for (sl = sa->spacedata.first; sl; sl = sl->next)
				if (sl->spacetype == SPACE_VIEW3D)
					BKE_screen_view3d_sync((View3D *)sl, scene);
	}
}

void BKE_screen_view3d_twmode_remove(View3D *v3d, const int i)
{
	const int selected_index = (v3d->twmode - V3D_MANIP_CUSTOM);
	if (selected_index == i) {
		v3d->twmode = V3D_MANIP_GLOBAL; /* fallback to global	*/
	}
	else if (selected_index > i) {
		v3d->twmode--;
	}
}

void BKE_screen_view3d_main_twmode_remove(ListBase *screen_lb, Scene *scene, const int i)
{
	bScreen *sc;

	for (sc = screen_lb->first; sc; sc = sc->id.next) {
		if (sc->scene == scene) {
			ScrArea *sa;
			for (sa = sc->areabase.first; sa; sa = sa->next) {
				SpaceLink *sl;
				for (sl = sa->spacedata.first; sl; sl = sl->next) {
					if (sl->spacetype == SPACE_VIEW3D) {
						View3D *v3d = (View3D *)sl;
						BKE_screen_view3d_twmode_remove(v3d, i);
					}
				}
			}
		}
	}
}

/* magic zoom calculation, no idea what
 * it signifies, if you find out, tell me! -zr
 */

/* simple, its magic dude!
 * well, to be honest, this gives a natural feeling zooming
 * with multiple keypad presses (ton)
 */
float BKE_screen_view3d_zoom_to_fac(float camzoom)
{
	return powf(((float)M_SQRT2 + camzoom / 50.0f), 2.0f) / 4.0f;
}

float BKE_screen_view3d_zoom_from_fac(float zoomfac)
{
	return ((sqrtf(4.0f * zoomfac) - (float)M_SQRT2) * 50.0f);
}

void BKE_screen_gpu_fx_validate(GPUFXSettings *fx_settings)
{
	/* currently we use DOF from the camera _only_,
	 * so we never allocate this, only copy from the Camera */
#if 0
	if ((fx_settings->dof == NULL) &&
	    (fx_settings->fx_flag & GPU_FX_FLAG_DOF))
	{
		GPUDOFSettings *fx_dof;
		fx_dof = fx_settings->dof = MEM_callocN(sizeof(GPUDOFSettings), __func__);
	}
#endif

	if ((fx_settings->ssao == NULL) &&
	    (fx_settings->fx_flag & GPU_FX_FLAG_SSAO))
	{
		GPUSSAOSettings *fx_ssao;
		fx_ssao = fx_settings->ssao = MEM_callocN(sizeof(GPUSSAOSettings), __func__);

		GPU_fx_compositor_init_ssao_settings(fx_ssao);
	}
}
