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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/manipulators/intern/wm_manipulatormap.c
 *  \ingroup wm
 */

#include <string.h>

#include "BKE_context.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_ghash.h"

#include "DNA_manipulator_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "GPU_glew.h"
#include "GPU_matrix.h"
#include "GPU_select.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_event_system.h"

/* own includes */
#include "wm_manipulator_wmapi.h"
#include "wm_manipulator_intern.h"

/**
 * Store all manipulator-maps here. Anyone who wants to register a manipulator for a certain
 * area type can query the manipulator-map to do so.
 */
static ListBase manipulatormaptypes = {NULL, NULL};

/**
 * Manipulator-map update tagging.
 */
enum eManipulatorMapUpdateFlags {
	/* Tag manipulator-map for refresh. */
	MANIPULATORMAP_REFRESH = (1 << 0),
};


/* -------------------------------------------------------------------- */
/** \name wmManipulatorMap
 *
 * \{ */

/**
 * Creates a manipulator-map with all registered manipulators for that type
 */
wmManipulatorMap *WM_manipulatormap_new_from_type(const struct wmManipulatorMapType_Params *mmap_params)
{
	wmManipulatorMapType *mmaptype = WM_manipulatormaptype_ensure(mmap_params);
	wmManipulatorMap *mmap;

	mmap = MEM_callocN(sizeof(wmManipulatorMap), "ManipulatorMap");
	mmap->type = mmaptype;
	mmap->update_flag = MANIPULATORMAP_REFRESH;

	/* create all manipulator-groups for this manipulator-map. We may create an empty one
	 * too in anticipation of manipulators from operators etc */
	for (wmManipulatorGroupType *mgrouptype = mmaptype->manipulator_grouptypes.first;
	     mgrouptype;
	     mgrouptype = mgrouptype->next)
	{
		wmManipulatorGroup *mgroup = wm_manipulatorgroup_new_from_type(mgrouptype);
		BLI_addtail(&mmap->manipulator_groups, mgroup);
	}

	return mmap;
}

void wm_manipulatormap_selected_delete(wmManipulatorMap *mmap)
{
	MEM_SAFE_FREE(mmap->mmap_context.selected_manipulator);
	mmap->mmap_context.tot_selected = 0;
}

void wm_manipulatormap_delete(wmManipulatorMap *mmap)
{
	if (!mmap)
		return;

	for (wmManipulatorGroup *mgroup = mmap->manipulator_groups.first, *mgroup_next; mgroup; mgroup = mgroup_next) {
		mgroup_next = mgroup->next;
		wm_manipulatorgroup_free(NULL, mmap, mgroup);
	}
	BLI_assert(BLI_listbase_is_empty(&mmap->manipulator_groups));

	wm_manipulatormap_selected_delete(mmap);

	MEM_freeN(mmap);
}

/**
 * Creates and returns idname hash table for (visible) manipulators in \a mmap
 *
 * \param poll  Polling function for excluding manipulators.
 * \param data  Custom data passed to \a poll
 */
static GHash *WM_manipulatormap_manipulator_hash_new(
        const bContext *C, wmManipulatorMap *mmap,
        bool (*poll)(const wmManipulator *, void *),
        void *data, const bool include_hidden)
{
	GHash *hash = BLI_ghash_str_new(__func__);

	/* collect manipulators */
	for (wmManipulatorGroup *mgroup = mmap->manipulator_groups.first; mgroup; mgroup = mgroup->next) {
		if (!mgroup->type->poll || mgroup->type->poll(C, mgroup->type)) {
			for (wmManipulator *manipulator = mgroup->manipulators.first;
			     manipulator;
			     manipulator = manipulator->next)
			{
				if ((include_hidden || (manipulator->flag & WM_MANIPULATOR_HIDDEN) == 0) &&
				    (!poll || poll(manipulator, data)))
				{
					BLI_ghash_insert(hash, manipulator->idname, manipulator);
				}
			}
		}
	}

	return hash;
}

void WM_manipulatormap_tag_refresh(wmManipulatorMap *mmap)
{
	if (mmap) {
		mmap->update_flag |= MANIPULATORMAP_REFRESH;
	}
}

static void manipulatormap_tag_updated(wmManipulatorMap *mmap)
{
	mmap->update_flag = 0;
}

static bool manipulator_prepare_drawing(
        wmManipulatorMap *mmap, wmManipulator *manipulator,
        const bContext *C, ListBase *draw_manipulators)
{
	if (!wm_manipulator_is_visible(manipulator)) {
		/* skip */
	}
	else {
		wm_manipulator_update(manipulator, C, (mmap->update_flag & MANIPULATORMAP_REFRESH) != 0);
		BLI_addhead(draw_manipulators, BLI_genericNodeN(manipulator));
		return true;
	}

	return false;
}

/**
 * Update manipulators of \a mmap to prepare for drawing. Adds all manipulators that
 * should be drawn to list \a draw_manipulators, note that added items need freeing.
 */
static void manipulatormap_prepare_drawing(
        wmManipulatorMap *mmap, const bContext *C, ListBase *draw_manipulators, const int drawstep)
{
	if (!mmap || BLI_listbase_is_empty(&mmap->manipulator_groups))
		return;
	wmManipulator *active_manipulator = mmap->mmap_context.active_manipulator;

	/* only active manipulator needs updating */
	if (active_manipulator) {
		if (manipulator_prepare_drawing(mmap, active_manipulator, C, draw_manipulators)) {
			manipulatormap_tag_updated(mmap);
		}
		/* don't draw any other manipulators */
		return;
	}

	for (wmManipulatorGroup *mgroup = mmap->manipulator_groups.first; mgroup; mgroup = mgroup->next) {
		/* check group visibility - drawstep first to avoid unnecessary call of group poll callback */
		if (!wm_manipulatorgroup_is_visible_in_drawstep(mgroup, drawstep) ||
		    !wm_manipulatorgroup_is_visible(mgroup, C))
		{
			continue;
		}

		/* needs to be initialized on first draw */
		wm_manipulatorgroup_ensure_initialized(mgroup, C);
		/* update data if needed */
		/* XXX weak: Manipulator-group may skip refreshing if it's invisible (map gets untagged nevertheless) */
		if (mmap->update_flag & MANIPULATORMAP_REFRESH && mgroup->type->refresh) {
			mgroup->type->refresh(C, mgroup);
		}
		/* prepare drawing */
		if (mgroup->type->draw_prepare) {
			mgroup->type->draw_prepare(C, mgroup);
		}

		for (wmManipulator *manipulator = mgroup->manipulators.first; manipulator; manipulator = manipulator->next) {
			manipulator_prepare_drawing(mmap, manipulator, C, draw_manipulators);
		}
	}

	manipulatormap_tag_updated(mmap);
}

/**
 * Draw all visible manipulators in \a mmap.
 * Uses global draw_manipulators listbase.
 */
static void manipulators_draw_list(const wmManipulatorMap *mmap, const bContext *C, ListBase *draw_manipulators)
{
	if (!mmap)
		return;
	BLI_assert(!BLI_listbase_is_empty(&mmap->manipulator_groups));

	const bool draw_multisample = (U.ogl_multisamples != USER_MULTISAMPLE_NONE);
	const bool use_lighting = (U.manipulator_flag & V3D_SHADED_MANIPULATORS) != 0;

	/* enable multisampling */
	if (draw_multisample) {
		glEnable(GL_MULTISAMPLE);
	}
	if (use_lighting) {
		const float lightpos[4] = {0.0, 0.0, 1.0, 0.0};
		const float diffuse[4] = {1.0, 1.0, 1.0, 0.0};

		glPushAttrib(GL_LIGHTING_BIT | GL_ENABLE_BIT);

		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_COLOR_MATERIAL);
		glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
		gpuPushMatrix();
		gpuLoadIdentity();
		glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
		gpuPopMatrix();
	}

	/* draw_manipulators contains all visible manipulators - draw them */
	for (LinkData *link = draw_manipulators->first, *link_next; link; link = link_next) {
		wmManipulator *manipulator = link->data;
		link_next = link->next;

		manipulator->draw(C, manipulator);
		/* free/remove manipulator link after drawing */
		BLI_freelinkN(draw_manipulators, link);
	}

	if (draw_multisample) {
		glDisable(GL_MULTISAMPLE);
	}
	if (use_lighting) {
		glPopAttrib();
	}
}

void WM_manipulatormap_draw(wmManipulatorMap *mmap, const bContext *C, const int drawstep)
{
	ListBase draw_manipulators = {NULL};

	manipulatormap_prepare_drawing(mmap, C, &draw_manipulators, drawstep);
	manipulators_draw_list(mmap, C, &draw_manipulators);
	BLI_assert(BLI_listbase_is_empty(&draw_manipulators));
}

static void manipulator_find_active_3D_loop(const bContext *C, ListBase *visible_manipulators)
{
	int selectionbase = 0;
	wmManipulator *manipulator;

	for (LinkData *link = visible_manipulators->first; link; link = link->next) {
		manipulator = link->data;
		/* pass the selection id shifted by 8 bits. Last 8 bits are used for selected manipulator part id */
		manipulator->render_3d_intersection(C, manipulator, selectionbase << 8);

		selectionbase++;
	}
}

static int manipulator_find_intersected_3D_intern(
        ListBase *visible_manipulators, const bContext *C, const int co[2],
        const float hotspot)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;
	rcti rect;
	GLuint buffer[64];      // max 4 items per select, so large enuf
	short hits;
	const bool do_passes = GPU_select_query_check_active();

	extern void view3d_winmatrix_set(ARegion *ar, View3D *v3d, const rcti *rect);

	rect.xmin = co[0] - hotspot;
	rect.xmax = co[0] + hotspot;
	rect.ymin = co[1] - hotspot;
	rect.ymax = co[1] + hotspot;

	view3d_winmatrix_set(ar, v3d, &rect);
	mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);

	if (do_passes)
		GPU_select_begin(buffer, ARRAY_SIZE(buffer), &rect, GPU_SELECT_NEAREST_FIRST_PASS, 0);
	else
		GPU_select_begin(buffer, ARRAY_SIZE(buffer), &rect, GPU_SELECT_ALL, 0);
	/* do the drawing */
	manipulator_find_active_3D_loop(C, visible_manipulators);

	hits = GPU_select_end();

	if (do_passes) {
		GPU_select_begin(buffer, ARRAY_SIZE(buffer), &rect, GPU_SELECT_NEAREST_SECOND_PASS, hits);
		manipulator_find_active_3D_loop(C, visible_manipulators);
		GPU_select_end();
	}

	view3d_winmatrix_set(ar, v3d, NULL);
	mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);

	return hits > 0 ? buffer[3] : -1;
}

/**
 * Try to find a 3D manipulator at screen-space coordinate \a co. Uses OpenGL picking.
 */
static wmManipulator *manipulator_find_intersected_3D(
        bContext *C, const int co[2], ListBase *visible_manipulators,
        unsigned char *part)
{
	wmManipulator *result = NULL;
	const float hotspot = 14.0f;
	int ret;

	*part = 0;
	/* set up view matrices */
	view3d_operator_needs_opengl(C);

	ret = manipulator_find_intersected_3D_intern(visible_manipulators, C, co, 0.5f * hotspot);

	if (ret != -1) {
		LinkData *link;
		int retsec;
		retsec = manipulator_find_intersected_3D_intern(visible_manipulators, C, co, 0.2f * hotspot);

		if (retsec != -1)
			ret = retsec;

		link = BLI_findlink(visible_manipulators, ret >> 8);
		*part = ret & 255;
		result = link->data;
	}

	return result;
}

/**
 * Try to find a manipulator under the mouse position. 2D intersections have priority over
 * 3D ones (could check for smallest screen-space distance but not needed right now).
 */
wmManipulator *wm_manipulatormap_find_highlighted_manipulator(
        wmManipulatorMap *mmap, bContext *C, const wmEvent *event,
        unsigned char *part)
{
	wmManipulator *manipulator = NULL;
	ListBase visible_3d_manipulators = {NULL};

	for (wmManipulatorGroup *mgroup = mmap->manipulator_groups.first; mgroup; mgroup = mgroup->next) {
		if (wm_manipulatorgroup_is_visible(mgroup, C)) {
			if (mgroup->type->flag & WM_MANIPULATORGROUPTYPE_IS_3D) {
				wm_manipulatorgroup_intersectable_manipulators_to_list(mgroup, &visible_3d_manipulators);
			}
			else if ((manipulator = wm_manipulatorgroup_find_intersected_mainpulator(mgroup, C, event, part))) {
				break;
			}
		}
	}

	if (!BLI_listbase_is_empty(&visible_3d_manipulators)) {
		manipulator = manipulator_find_intersected_3D(C, event->mval, &visible_3d_manipulators, part);
		BLI_freelistN(&visible_3d_manipulators);
	}

	return manipulator;
}

void WM_manipulatormap_add_handlers(ARegion *ar, wmManipulatorMap *mmap)
{
	wmEventHandler *handler = MEM_callocN(sizeof(wmEventHandler), "manipulator handler");

	BLI_assert(mmap == ar->manipulator_map);
	handler->manipulator_map = mmap;
	BLI_addtail(&ar->handlers, handler);
}

void wm_manipulatormaps_handled_modal_update(
        bContext *C, wmEvent *event, wmEventHandler *handler,
        const wmOperatorType *ot)
{
	const bool modal_running = (handler->op != NULL);

	/* happens on render or when joining areas */
	if (!handler->op_region || !handler->op_region->manipulator_map)
		return;

	/* hide operator manipulators */
	if (!modal_running && ot->mgrouptype) {
		ot->mgrouptype->op = NULL;
	}

	wmManipulatorMap *mmap = handler->op_region->manipulator_map;
	wmManipulator *manipulator = wm_manipulatormap_get_active_manipulator(mmap);
	ScrArea *area = CTX_wm_area(C);
	ARegion *region = CTX_wm_region(C);

	wm_manipulatormap_handler_context(C, handler);

	/* regular update for running operator */
	if (modal_running) {
		if (manipulator && manipulator->handler && manipulator->opname &&
			STREQ(manipulator->opname, handler->op->idname))
		{
			manipulator->handler(C, event, manipulator, 0);
		}
	}
	/* operator not running anymore */
	else {
		wm_manipulatormap_set_highlighted_manipulator(mmap, C, NULL, 0);
		wm_manipulatormap_set_active_manipulator(mmap, C, event, NULL);
	}

	/* restore the area */
	CTX_wm_area_set(C, area);
	CTX_wm_region_set(C, region);
}

/**
 * Deselect all selected manipulators in \a mmap.
 * \return if selection has changed.
 */
bool wm_manipulatormap_deselect_all(wmManipulatorMap *mmap, wmManipulator ***sel)
{
	if (*sel == NULL || mmap->mmap_context.tot_selected == 0)
		return false;

	for (int i = 0; i < mmap->mmap_context.tot_selected; i++) {
		(*sel)[i]->state &= ~WM_MANIPULATOR_SELECTED;
		(*sel)[i] = NULL;
	}
	wm_manipulatormap_selected_delete(mmap);

	/* always return true, we already checked
	 * if there's anything to deselect */
	return true;
}

BLI_INLINE bool manipulator_selectable_poll(const wmManipulator *manipulator, void *UNUSED(data))
{
	return (manipulator->mgroup->type->flag & WM_MANIPULATORGROUPTYPE_SELECTABLE);
}

/**
 * Select all selectable manipulators in \a mmap.
 * \return if selection has changed.
 */
static bool wm_manipulatormap_select_all_intern(
        bContext *C, wmManipulatorMap *mmap, wmManipulator ***sel,
        const int action)
{
	/* GHash is used here to avoid having to loop over all manipulators twice (once to
	 * get tot_sel for allocating, once for actually selecting). Instead we collect
	 * selectable manipulators in hash table and use this to get tot_sel and do selection */

	GHash *hash = WM_manipulatormap_manipulator_hash_new(C, mmap, manipulator_selectable_poll, NULL, true);
	GHashIterator gh_iter;
	int i, *tot_sel = &mmap->mmap_context.tot_selected;
	bool changed = false;

	*tot_sel = BLI_ghash_size(hash);
	*sel = MEM_reallocN(*sel, sizeof(**sel) * (*tot_sel));

	GHASH_ITER_INDEX (gh_iter, hash, i) {
		wmManipulator *manipulator_iter = BLI_ghashIterator_getValue(&gh_iter);

		if ((manipulator_iter->state & WM_MANIPULATOR_SELECTED) == 0) {
			changed = true;
		}
		manipulator_iter->state |= WM_MANIPULATOR_SELECTED;
		if (manipulator_iter->select) {
			manipulator_iter->select(C, manipulator_iter, action);
		}
		(*sel)[i] = manipulator_iter;
		BLI_assert(i < (*tot_sel));
	}
	/* highlight first manipulator */
	wm_manipulatormap_set_highlighted_manipulator(mmap, C, (*sel)[0], (*sel)[0]->highlighted_part);

	BLI_ghash_free(hash, NULL, NULL);
	return changed;
}

/**
 * Select/Deselect all selectable manipulators in \a mmap.
 * \return if selection has changed.
 *
 * TODO select all by type
 */
bool WM_manipulatormap_select_all(bContext *C, wmManipulatorMap *mmap, const int action)
{
	wmManipulator ***sel = &mmap->mmap_context.selected_manipulator;
	bool changed = false;

	switch (action) {
		case SEL_SELECT:
			changed = wm_manipulatormap_select_all_intern(C, mmap, sel, action);
			break;
		case SEL_DESELECT:
			changed = wm_manipulatormap_deselect_all(mmap, sel);
			break;
		default:
			BLI_assert(0);
			break;
	}

	if (changed)
		WM_event_add_mousemove(C);

	return changed;
}

/**
 * Prepare context for manipulator handling (but only if area/region is
 * part of screen). Version of #wm_handler_op_context for manipulators.
 */
void wm_manipulatormap_handler_context(bContext *C, wmEventHandler *handler)
{
	bScreen *screen = CTX_wm_screen(C);

	if (screen) {
		if (handler->op_area == NULL) {
			/* do nothing in this context */
		}
		else {
			ScrArea *sa;

			for (sa = screen->areabase.first; sa; sa = sa->next)
				if (sa == handler->op_area)
					break;
			if (sa == NULL) {
				/* when changing screen layouts with running modal handlers (like render display), this
				 * is not an error to print */
				if (handler->manipulator_map == NULL)
					printf("internal error: modal manipulator-map handler has invalid area\n");
			}
			else {
				ARegion *ar;
				CTX_wm_area_set(C, sa);
				for (ar = sa->regionbase.first; ar; ar = ar->next)
					if (ar == handler->op_region)
						break;
				/* XXX no warning print here, after full-area and back regions are remade */
				if (ar)
					CTX_wm_region_set(C, ar);
			}
		}
	}
}

bool WM_manipulatormap_cursor_set(const wmManipulatorMap *mmap, wmWindow *win)
{
	for (; mmap; mmap = mmap->next) {
		wmManipulator *manipulator = mmap->mmap_context.highlighted_manipulator;
		if (manipulator && manipulator->get_cursor) {
			WM_cursor_set(win, manipulator->get_cursor(manipulator));
			return true;
		}
	}

	return false;
}

void wm_manipulatormap_set_highlighted_manipulator(
        wmManipulatorMap *mmap, const bContext *C, wmManipulator *manipulator,
        unsigned char part)
{
	if ((manipulator != mmap->mmap_context.highlighted_manipulator) ||
	    (manipulator && part != manipulator->highlighted_part))
	{
		if (mmap->mmap_context.highlighted_manipulator) {
			mmap->mmap_context.highlighted_manipulator->state &= ~WM_MANIPULATOR_HIGHLIGHT;
			mmap->mmap_context.highlighted_manipulator->highlighted_part = 0;
		}

		mmap->mmap_context.highlighted_manipulator = manipulator;

		if (manipulator) {
			manipulator->state |= WM_MANIPULATOR_HIGHLIGHT;
			manipulator->highlighted_part = part;

			if (C && manipulator->get_cursor) {
				wmWindow *win = CTX_wm_window(C);
				WM_cursor_set(win, manipulator->get_cursor(manipulator));
			}
		}
		else {
			if (C) {
				wmWindow *win = CTX_wm_window(C);
				WM_cursor_set(win, CURSOR_STD);
			}
		}

		/* tag the region for redraw */
		if (C) {
			ARegion *ar = CTX_wm_region(C);
			ED_region_tag_redraw(ar);
		}
	}
}

wmManipulator *wm_manipulatormap_get_highlighted_manipulator(wmManipulatorMap *mmap)
{
	return mmap->mmap_context.highlighted_manipulator;
}

void wm_manipulatormap_set_active_manipulator(
        wmManipulatorMap *mmap, bContext *C, const wmEvent *event, wmManipulator *manipulator)
{
	if (manipulator && C) {
		manipulator->state |= WM_MANIPULATOR_ACTIVE;
		mmap->mmap_context.active_manipulator = manipulator;

		if (manipulator->opname) {
			wmOperatorType *ot = WM_operatortype_find(manipulator->opname, 0);

			if (ot) {
				/* first activate the manipulator itself */
				if (manipulator->invoke && manipulator->handler) {
					manipulator->invoke(C, event, manipulator);
				}

				WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &manipulator->opptr);

				/* we failed to hook the manipulator to the operator handler or operator was cancelled, return */
				if (!mmap->mmap_context.active_manipulator) {
					manipulator->state &= ~WM_MANIPULATOR_ACTIVE;
					/* first activate the manipulator itself */
					if (manipulator->interaction_data) {
						MEM_freeN(manipulator->interaction_data);
						manipulator->interaction_data = NULL;
					}
				}
				return;
			}
			else {
				printf("Manipulator error: operator not found");
				mmap->mmap_context.active_manipulator = NULL;
				return;
			}
		}
		else {
			if (manipulator->invoke && manipulator->handler) {
				manipulator->invoke(C, event, manipulator);
			}
		}
		WM_cursor_grab_enable(CTX_wm_window(C), true, true, NULL);
	}
	else {
		manipulator = mmap->mmap_context.active_manipulator;


		/* deactivate, manipulator but first take care of some stuff */
		if (manipulator) {
			manipulator->state &= ~WM_MANIPULATOR_ACTIVE;
			/* first activate the manipulator itself */
			if (manipulator->interaction_data) {
				MEM_freeN(manipulator->interaction_data);
				manipulator->interaction_data = NULL;
			}
		}
		mmap->mmap_context.active_manipulator = NULL;

		if (C) {
			WM_cursor_grab_disable(CTX_wm_window(C), NULL);
			ED_region_tag_redraw(CTX_wm_region(C));
			WM_event_add_mousemove(C);
		}
	}
}

wmManipulator *wm_manipulatormap_get_active_manipulator(wmManipulatorMap *mmap)
{
	return mmap->mmap_context.active_manipulator;
}

/** \} */ /* wmManipulatorMap */


/* -------------------------------------------------------------------- */
/** \name wmManipulatorMapType
 *
 * \{ */

wmManipulatorMapType *WM_manipulatormaptype_find(
        const struct wmManipulatorMapType_Params *mmap_params)
{
	for (wmManipulatorMapType *mmaptype = manipulatormaptypes.first; mmaptype; mmaptype = mmaptype->next) {
		if (mmaptype->spaceid == mmap_params->spaceid &&
		    mmaptype->regionid == mmap_params->regionid &&
		    STREQ(mmaptype->idname, mmap_params->idname))
		{
			return mmaptype;
		}
	}

	return NULL;
}

wmManipulatorMapType *WM_manipulatormaptype_ensure(
        const struct wmManipulatorMapType_Params *mmap_params)
{
	wmManipulatorMapType *mmaptype = WM_manipulatormaptype_find(mmap_params);

	if (mmaptype) {
		return mmaptype;
	}

	mmaptype = MEM_callocN(sizeof(wmManipulatorMapType), "manipulatortype list");
	mmaptype->spaceid = mmap_params->spaceid;
	mmaptype->regionid = mmap_params->regionid;
	BLI_strncpy(mmaptype->idname, mmap_params->idname, sizeof(mmaptype->idname));
	BLI_addhead(&manipulatormaptypes, mmaptype);

	return mmaptype;
}

void wm_manipulatormaptypes_free(void)
{
	for (wmManipulatorMapType *mmaptype = manipulatormaptypes.first; mmaptype; mmaptype = mmaptype->next) {
		BLI_freelistN(&mmaptype->manipulator_grouptypes);
	}
	BLI_freelistN(&manipulatormaptypes);
}

/**
 * Initialize keymaps for all existing manipulator-groups
 */
void wm_manipulators_keymap(wmKeyConfig *keyconf)
{
	wmManipulatorMapType *mmaptype;
	wmManipulatorGroupType *mgrouptype;

	/* we add this item-less keymap once and use it to group manipulator-group keymaps into it */
	WM_keymap_find(keyconf, "Manipulators", 0, 0);

	for (mmaptype = manipulatormaptypes.first; mmaptype; mmaptype = mmaptype->next) {
		for (mgrouptype = mmaptype->manipulator_grouptypes.first; mgrouptype; mgrouptype = mgrouptype->next) {
			wm_manipulatorgrouptype_keymap_init(mgrouptype, keyconf);
		}
	}
}

/** \} */ /* wmManipulatorMapType */

