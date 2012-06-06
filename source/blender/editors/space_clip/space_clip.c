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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_clip/space_clip.c
 *  \ingroup spclip
 */

#include <string.h>
#include <stdio.h>

#include "DNA_scene_types.h"
#include "DNA_mask_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_view3d_types.h"	/* for pivot point */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_main.h"
#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "IMB_imbuf_types.h"

#include "ED_mask.h"
#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_clip.h"
#include "ED_transform.h"

#include "IMB_imbuf.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"


#include "clip_intern.h"	// own include

static void init_preview_region(const bContext *C, ARegion *ar)
{
	Scene *scene = CTX_data_scene(C);
	ScrArea *sa = CTX_wm_area(C);
	SpaceClip *sc = CTX_wm_space_clip(C);

	ar->regiontype = RGN_TYPE_PREVIEW;
	ar->alignment = RGN_ALIGN_TOP;
	ar->flag |= RGN_FLAG_HIDDEN;

	if (sc->view == SC_VIEW_DOPESHEET) {
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
	}
	else {
		ar->v2d.tot.xmin = 0.0f;
		ar->v2d.tot.ymin = -10.0f;
		ar->v2d.tot.xmax = (float)scene->r.efra;
		ar->v2d.tot.ymax = 10.0f;

		ar->v2d.cur = ar->v2d.tot;

		ar->v2d.min[0] = FLT_MIN;
		ar->v2d.min[1] = FLT_MIN;

		ar->v2d.max[0] = MAXFRAMEF;
		ar->v2d.max[1] = FLT_MAX;

		ar->v2d.scroll = (V2D_SCROLL_BOTTOM | V2D_SCROLL_SCALE_HORIZONTAL);
		ar->v2d.scroll |= (V2D_SCROLL_LEFT | V2D_SCROLL_SCALE_VERTICAL);

		ar->v2d.minzoom = 0.0f;
		ar->v2d.maxzoom = 0.0f;
		ar->v2d.keepzoom = 0;
		ar->v2d.keepofs = 0;
		ar->v2d.align = 0;
		ar->v2d.flag = 0;

		ar->v2d.keeptot = 0;
	}
}

static void reinit_preview_region(const bContext *C, ARegion *ar)
{
	SpaceClip *sc = CTX_wm_space_clip(C);

	if (sc->view == SC_VIEW_DOPESHEET) {
		if ((ar->v2d.flag & V2D_VIEWSYNC_AREA_VERTICAL) == 0)
			init_preview_region(C, ar);
	}
	else {
		if (ar->v2d.flag & V2D_VIEWSYNC_AREA_VERTICAL)
			init_preview_region(C, ar);
	}
}

static ARegion *ED_clip_has_preview_region(const bContext *C, ScrArea *sa)
{
	ARegion *ar, *arnew;

	ar = BKE_area_find_region_type(sa, RGN_TYPE_PREVIEW);
	if (ar)
		return ar;

	/* add subdiv level; after header */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);

	/* is error! */
	if (ar == NULL)
		return NULL;

	arnew = MEM_callocN(sizeof(ARegion), "clip preview region");

	BLI_insertlinkbefore(&sa->regionbase, ar, arnew);
	init_preview_region(C, arnew);

	return arnew;
}

static ARegion *ED_clip_has_channels_region(ScrArea *sa)
{
	ARegion *ar, *arnew;

	ar = BKE_area_find_region_type(sa, RGN_TYPE_CHANNELS);
	if (ar)
		return ar;

	/* add subdiv level; after header */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_PREVIEW);

	/* is error! */
	if (ar == NULL)
		return NULL;

	arnew = MEM_callocN(sizeof(ARegion), "clip channels region");

	BLI_insertlinkbefore(&sa->regionbase, ar, arnew);
	arnew->regiontype = RGN_TYPE_CHANNELS;
	arnew->alignment = RGN_ALIGN_LEFT;

	arnew->v2d.scroll = V2D_SCROLL_BOTTOM;
	arnew->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

	return arnew;
}

static void clip_scopes_tag_refresh(ScrArea *sa)
{
	SpaceClip *sc = (SpaceClip *)sa->spacedata.first;
	ARegion *ar;

	if (sc->mode != SC_MODE_TRACKING)
		return;

	/* only while proeprties are visible */
	for (ar = sa->regionbase.first; ar; ar = ar->next) {
		if (ar->regiontype == RGN_TYPE_UI && ar->flag & RGN_FLAG_HIDDEN)
			return;
	}

	sc->scopes.ok = FALSE;
}

static void clip_stabilization_tag_refresh(ScrArea *sa)
{
	SpaceClip *sc = (SpaceClip *) sa->spacedata.first;
	MovieClip *clip = ED_space_clip(sc);

	if (clip) {
		MovieTrackingStabilization *stab = &clip->tracking.stabilization;

		stab->ok = FALSE;
	}
}

/* ******************** default callbacks for clip space ***************** */

static SpaceLink *clip_new(const bContext *C)
{
	ARegion *ar;
	SpaceClip *sc;

	sc = MEM_callocN(sizeof(SpaceClip), "initclip");
	sc->spacetype = SPACE_CLIP;
	sc->flag = SC_SHOW_MARKER_PATTERN | SC_SHOW_TRACK_PATH | SC_MANUAL_CALIBRATION |
	           SC_SHOW_GRAPH_TRACKS | SC_SHOW_GRAPH_FRAMES;
	sc->zoom = 1.0f;
	sc->path_length = 20;
	sc->scopes.track_preview_height = 120;
	sc->around = V3D_LOCAL;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for clip");

	BLI_addtail(&sc->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;

	/* tools view */
	ar = MEM_callocN(sizeof(ARegion), "tools for clip");

	BLI_addtail(&sc->regionbase, ar);
	ar->regiontype = RGN_TYPE_TOOLS;
	ar->alignment = RGN_ALIGN_LEFT;

	/* tool properties */
	ar = MEM_callocN(sizeof(ARegion), "tool properties for clip");

	BLI_addtail(&sc->regionbase, ar);
	ar->regiontype = RGN_TYPE_TOOL_PROPS;
	ar->alignment = RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV;

	/* properties view */
	ar = MEM_callocN(sizeof(ARegion), "properties for clip");

	BLI_addtail(&sc->regionbase, ar);
	ar->regiontype = RGN_TYPE_UI;
	ar->alignment = RGN_ALIGN_RIGHT;

	/* channels view */
	ar = MEM_callocN(sizeof(ARegion), "channels for clip");

	BLI_addtail(&sc->regionbase, ar);
	ar->regiontype = RGN_TYPE_CHANNELS;
	ar->alignment = RGN_ALIGN_LEFT;

	ar->v2d.scroll = V2D_SCROLL_BOTTOM;
	ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

	/* preview view */
	ar = MEM_callocN(sizeof(ARegion), "preview for clip");

	BLI_addtail(&sc->regionbase, ar);
	init_preview_region(C, ar);

	/* main area */
	ar = MEM_callocN(sizeof(ARegion), "main area for clip");

	BLI_addtail(&sc->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;

	return (SpaceLink *) sc;
}

/* not spacelink itself */
static void clip_free(SpaceLink *sl)
{
	SpaceClip *sc = (SpaceClip*) sl;

	sc->clip = NULL;

	if (sc->scopes.track_preview)
		IMB_freeImBuf(sc->scopes.track_preview);

	ED_space_clip_free_texture_buffer(sc);
}

/* spacetype; init callback */
static void clip_init(struct wmWindowManager *UNUSED(wm), ScrArea *sa)
{
	ListBase *lb = WM_dropboxmap_find("Clip", SPACE_CLIP, 0);

	/* add drop boxes */
	WM_event_add_dropbox_handler(&sa->handlers, lb);
}

static SpaceLink *clip_duplicate(SpaceLink *sl)
{
	SpaceClip *scn = MEM_dupallocN(sl);

	/* clear or remove stuff from old */
	scn->scopes.track_preview = NULL;
	scn->scopes.ok = FALSE;
	scn->draw_context = NULL;

	return (SpaceLink *)scn;
}

static void clip_listener(ScrArea *sa, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCENE:
			switch (wmn->data) {
				case ND_FRAME:
					clip_scopes_tag_refresh(sa);
					/* no break! */

				case ND_FRAME_RANGE:
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_MOVIECLIP:
			switch (wmn->data) {
				case ND_DISPLAY:
				case ND_SELECT:
					clip_scopes_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
			}
			switch (wmn->action) {
				case NA_REMOVED:
				case NA_EDITED:
				case NA_EVALUATED:
					clip_stabilization_tag_refresh(sa);
					/* no break! */

				case NA_SELECTED:
					clip_scopes_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_MASK:
			switch (wmn->data) {
				case ND_SELECT:
				case ND_DATA:
				case ND_DRAW:
					ED_area_tag_redraw(sa);
					break;
			}
			switch (wmn->action) {
				case NA_SELECTED:
					clip_scopes_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
				case NA_EDITED:
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_GEOM:
			switch (wmn->data) {
				case ND_SELECT:
					clip_scopes_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_SCREEN:
			if (wmn->data == ND_ANIMPLAY) {
				ED_area_tag_redraw(sa);
			}
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_CLIP) {
				clip_scopes_tag_refresh(sa);
				clip_stabilization_tag_refresh(sa);
				ED_area_tag_redraw(sa);
			}
			break;
	}
}

static void clip_operatortypes(void)
{
	/* ** clip_ops.c ** */
	WM_operatortype_append(CLIP_OT_open);
	WM_operatortype_append(CLIP_OT_reload);
	WM_operatortype_append(CLIP_OT_view_pan);
	WM_operatortype_append(CLIP_OT_view_zoom);
	WM_operatortype_append(CLIP_OT_view_zoom_in);
	WM_operatortype_append(CLIP_OT_view_zoom_out);
	WM_operatortype_append(CLIP_OT_view_zoom_ratio);
	WM_operatortype_append(CLIP_OT_view_all);
	WM_operatortype_append(CLIP_OT_view_selected);
	WM_operatortype_append(CLIP_OT_change_frame);
	WM_operatortype_append(CLIP_OT_rebuild_proxy);
	WM_operatortype_append(CLIP_OT_mode_set);

	/* ** clip_toolbar.c ** */
	WM_operatortype_append(CLIP_OT_tools);
	WM_operatortype_append(CLIP_OT_properties);

	/* ** tracking_ops.c ** */

	/* navigation */
	WM_operatortype_append(CLIP_OT_frame_jump);

	/* foorage */
	WM_operatortype_append(CLIP_OT_set_center_principal);

	/* selection */
	WM_operatortype_append(CLIP_OT_select);
	WM_operatortype_append(CLIP_OT_select_all);
	WM_operatortype_append(CLIP_OT_select_border);
	WM_operatortype_append(CLIP_OT_select_lasso);
	WM_operatortype_append(CLIP_OT_select_circle);
	WM_operatortype_append(CLIP_OT_select_grouped);

	/* markers */
	WM_operatortype_append(CLIP_OT_add_marker);
	WM_operatortype_append(CLIP_OT_slide_marker);
	WM_operatortype_append(CLIP_OT_delete_track);
	WM_operatortype_append(CLIP_OT_delete_marker);

	/* track */
	WM_operatortype_append(CLIP_OT_track_markers);

	/* solving */
	WM_operatortype_append(CLIP_OT_solve_camera);
	WM_operatortype_append(CLIP_OT_clear_solution);

	WM_operatortype_append(CLIP_OT_disable_markers);
	WM_operatortype_append(CLIP_OT_hide_tracks);
	WM_operatortype_append(CLIP_OT_hide_tracks_clear);
	WM_operatortype_append(CLIP_OT_lock_tracks);

	/* orientation */
	WM_operatortype_append(CLIP_OT_set_origin);
	WM_operatortype_append(CLIP_OT_set_plane);
	WM_operatortype_append(CLIP_OT_set_axis);
	WM_operatortype_append(CLIP_OT_set_scale);
	WM_operatortype_append(CLIP_OT_set_solution_scale);

	/* detect */
	WM_operatortype_append(CLIP_OT_detect_features);

	/* stabilization */
	WM_operatortype_append(CLIP_OT_stabilize_2d_add);
	WM_operatortype_append(CLIP_OT_stabilize_2d_remove);
	WM_operatortype_append(CLIP_OT_stabilize_2d_select);
	WM_operatortype_append(CLIP_OT_stabilize_2d_set_rotation);

	/* clean-up */
	WM_operatortype_append(CLIP_OT_clear_track_path);
	WM_operatortype_append(CLIP_OT_join_tracks);
	WM_operatortype_append(CLIP_OT_track_copy_color);

	WM_operatortype_append(CLIP_OT_clean_tracks);

	/* object tracking */
	WM_operatortype_append(CLIP_OT_tracking_object_new);
	WM_operatortype_append(CLIP_OT_tracking_object_remove);

	/* clipboard */
	WM_operatortype_append(CLIP_OT_copy_tracks);
	WM_operatortype_append(CLIP_OT_paste_tracks);

	/* ** clip_graph_ops.c  ** */

	/* graph editing */

	/* selection */
	WM_operatortype_append(CLIP_OT_graph_select);
	WM_operatortype_append(CLIP_OT_graph_select_border);
	WM_operatortype_append(CLIP_OT_graph_select_all_markers);

	WM_operatortype_append(CLIP_OT_graph_delete_curve);
	WM_operatortype_append(CLIP_OT_graph_delete_knot);
	WM_operatortype_append(CLIP_OT_graph_view_all);
	WM_operatortype_append(CLIP_OT_graph_center_current_frame);

	WM_operatortype_append(CLIP_OT_graph_disable_markers);

	/* ** clip_dopesheet_ops.c  ** */

	WM_operatortype_append(CLIP_OT_dopesheet_select_channel);
}

static void clip_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;

	/* ******** Global hotkeys avalaible for all regions ******** */

	keymap = WM_keymap_find(keyconf, "Clip", SPACE_CLIP, 0);

	WM_keymap_add_item(keymap, "CLIP_OT_open", OKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "CLIP_OT_tools", TKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_properties", NKEY, KM_PRESS, 0, 0);

	/* 2d tracking */
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_track_markers", LEFTARROWKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "backwards", TRUE);
	RNA_boolean_set(kmi->ptr, "sequence", FALSE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_track_markers", RIGHTARROWKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "backwards", FALSE);
	RNA_boolean_set(kmi->ptr, "sequence", FALSE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_track_markers", TKEY, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "backwards", FALSE);
	RNA_boolean_set(kmi->ptr, "sequence", TRUE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_track_markers", TKEY, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "backwards", TRUE);
	RNA_boolean_set(kmi->ptr, "sequence", TRUE);

	/* mode */
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_mode_set", TABKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "mode", SC_MODE_RECONSTRUCTION);
	RNA_boolean_set(kmi->ptr, "toggle", TRUE);

	kmi = WM_keymap_add_item(keymap, "CLIP_OT_mode_set", TABKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "mode", SC_MODE_DISTORTION);
	RNA_boolean_set(kmi->ptr, "toggle", TRUE);

	WM_keymap_add_item(keymap, "CLIP_OT_solve_camera", SKEY, KM_PRESS, KM_SHIFT, 0);

	/* ******** Hotkeys avalaible for main region only ******** */

	keymap = WM_keymap_find(keyconf, "Clip Editor", SPACE_CLIP, 0);
//	keymap->poll = ED_space_clip_tracking_poll;
	/* ** View/navigation ** */

	WM_keymap_add_item(keymap, "CLIP_OT_view_pan", MIDDLEMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_view_pan", MIDDLEMOUSE, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_view_pan", MOUSEPAN, 0, 0, 0);

	WM_keymap_add_item(keymap, "CLIP_OT_view_zoom", MIDDLEMOUSE, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_view_zoom", MOUSEZOOM, 0, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_view_zoom_in", WHEELINMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_view_zoom_out", WHEELOUTMOUSE, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_view_zoom_in", PADPLUSKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_view_zoom_out", PADMINUS, KM_PRESS, 0, 0);

	RNA_float_set(WM_keymap_add_item(keymap, "CLIP_OT_view_zoom_ratio", PAD8, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 8.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "CLIP_OT_view_zoom_ratio", PAD4, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 4.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "CLIP_OT_view_zoom_ratio", PAD2, KM_PRESS, KM_SHIFT, 0)->ptr, "ratio", 2.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "CLIP_OT_view_zoom_ratio", PAD1, KM_PRESS, 0, 0)->ptr, "ratio", 1.0f);
	RNA_float_set(WM_keymap_add_item(keymap, "CLIP_OT_view_zoom_ratio", PAD2, KM_PRESS, 0, 0)->ptr, "ratio", 0.5f);
	RNA_float_set(WM_keymap_add_item(keymap, "CLIP_OT_view_zoom_ratio", PAD4, KM_PRESS, 0, 0)->ptr, "ratio", 0.25f);
	RNA_float_set(WM_keymap_add_item(keymap, "CLIP_OT_view_zoom_ratio", PAD8, KM_PRESS, 0, 0)->ptr, "ratio", 0.125f);

	WM_keymap_add_item(keymap, "CLIP_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "CLIP_OT_view_all", FKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "fit_view", TRUE);

	WM_keymap_add_item(keymap, "CLIP_OT_view_selected", PADPERIOD, KM_PRESS, 0, 0);

	/* jump to special frame */
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_frame_jump", LEFTARROWKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "position", 0);

	kmi = WM_keymap_add_item(keymap, "CLIP_OT_frame_jump", RIGHTARROWKEY, KM_PRESS, KM_CTRL | KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "position", 1);

	kmi = WM_keymap_add_item(keymap, "CLIP_OT_frame_jump", LEFTARROWKEY, KM_PRESS, KM_ALT | KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "position", 2);

	kmi = WM_keymap_add_item(keymap, "CLIP_OT_frame_jump", RIGHTARROWKEY, KM_PRESS, KM_ALT | KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "position", 3);

	/* "timeline" */
	WM_keymap_add_item(keymap, "CLIP_OT_change_frame", LEFTMOUSE, KM_PRESS, 0, 0);

	/* selection */
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", FALSE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_select_all", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_TOGGLE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_select_all", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);
	WM_keymap_add_item(keymap, "CLIP_OT_select_border", BKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_select_circle", CKEY, KM_PRESS, 0, 0);
	WM_keymap_add_menu(keymap, "CLIP_MT_select_grouped", GKEY, KM_PRESS, KM_SHIFT, 0);

	kmi = WM_keymap_add_item(keymap, "CLIP_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", FALSE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_select_lasso", EVT_TWEAK_A, KM_ANY, KM_CTRL | KM_SHIFT | KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "deselect", TRUE);

	/* marker */
	WM_keymap_add_item(keymap, "CLIP_OT_add_marker_slide", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "CLIP_OT_delete_marker", DELKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_delete_marker", XKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_add_item(keymap, "CLIP_OT_slide_marker", LEFTMOUSE, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "CLIP_OT_disable_markers", DKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "action", 2);	/* toggle */

	/* tracks */
	WM_keymap_add_item(keymap, "CLIP_OT_delete_track", DELKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_delete_track", XKEY, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "CLIP_OT_lock_tracks", LKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", 0);	/* lock */

	kmi = WM_keymap_add_item(keymap, "CLIP_OT_lock_tracks", LKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "action", 1);	/* unlock */

	kmi = WM_keymap_add_item(keymap, "CLIP_OT_hide_tracks", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", FALSE);

	kmi = WM_keymap_add_item(keymap, "CLIP_OT_hide_tracks", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", TRUE);

	WM_keymap_add_item(keymap, "CLIP_OT_hide_tracks_clear", HKEY, KM_PRESS, KM_ALT, 0);

	/* clean-up */
	WM_keymap_add_item(keymap, "CLIP_OT_join_tracks", JKEY, KM_PRESS, KM_CTRL, 0);

	/* menus */
	WM_keymap_add_menu(keymap, "CLIP_MT_tracking_specials", WKEY, KM_PRESS, 0, 0);

	/* display */
	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", LKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.lock_selection");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", DKEY, KM_PRESS, KM_ALT, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.show_disabled");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", SKEY, KM_PRESS, KM_ALT, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.show_marker_search");

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", MKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.use_mute_footage");

	transform_keymap_for_space(keyconf, keymap, SPACE_CLIP);

	/* clean-up */
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_clear_track_path", TKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "action", TRACK_CLEAR_REMAINED);
	RNA_boolean_set(kmi->ptr, "clear_active", FALSE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_clear_track_path", TKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "action", TRACK_CLEAR_UPTO);
	RNA_boolean_set(kmi->ptr, "clear_active", FALSE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_clear_track_path", TKEY, KM_PRESS, KM_ALT | KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "action", TRACK_CLEAR_ALL);
	RNA_boolean_set(kmi->ptr, "clear_active", FALSE);

	/* ******** Hotkeys avalaible for preview region only ******** */

	keymap = WM_keymap_find(keyconf, "Clip Graph Editor", SPACE_CLIP, 0);

	/* "timeline" */
	WM_keymap_add_item(keymap, "CLIP_OT_change_frame", ACTIONMOUSE, KM_PRESS, 0, 0);

	/* selection */
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_graph_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", FALSE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_graph_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);

	kmi = WM_keymap_add_item(keymap, "CLIP_OT_graph_select_all_markers", AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_TOGGLE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_graph_select_all_markers", IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);

	WM_keymap_add_item(keymap, "CLIP_OT_graph_select_border", BKEY, KM_PRESS, 0, 0);

	/* delete */
	WM_keymap_add_item(keymap, "CLIP_OT_graph_delete_curve", DELKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_graph_delete_curve", XKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "CLIP_OT_graph_delete_knot", DELKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_graph_delete_knot", XKEY, KM_PRESS, KM_SHIFT, 0);

	/* view */
	WM_keymap_add_item(keymap, "CLIP_OT_graph_view_all", HOMEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_graph_center_current_frame", PADPERIOD, KM_PRESS, 0, 0);

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", LKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.lock_time_cursor");

	/* clean-up */
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_clear_track_path", TKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "action", TRACK_CLEAR_REMAINED);
	RNA_boolean_set(kmi->ptr, "clear_active", TRUE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_clear_track_path", TKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "action", TRACK_CLEAR_UPTO);
	RNA_boolean_set(kmi->ptr, "clear_active", TRUE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_clear_track_path", TKEY, KM_PRESS, KM_ALT | KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "action", TRACK_CLEAR_ALL);
	RNA_boolean_set(kmi->ptr, "clear_active", TRUE);

	/* tracks */
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_graph_disable_markers", DKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "action", 2);	/* toggle */

	transform_keymap_for_space(keyconf, keymap, SPACE_CLIP);

	/* ******** Hotkeys avalaible for channels region only ******** */

	keymap = WM_keymap_find(keyconf, "Clip Dopesheet Editor", SPACE_CLIP, 0);

	kmi = WM_keymap_add_item(keymap, "CLIP_OT_dopesheet_select_channel", ACTIONMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);	/* toggle */
}

const char *clip_context_dir[] = {"edit_movieclip", "edit_mask", NULL};

static int clip_context(const bContext *C, const char *member, bContextDataResult *result)
{
	SpaceClip *sc = CTX_wm_space_clip(C);

	if (CTX_data_dir(member)) {
		CTX_data_dir_set(result, clip_context_dir);

		return TRUE;
	}
	else if (CTX_data_equals(member, "edit_movieclip")) {
		if (sc->clip)
			CTX_data_id_pointer_set(result, &sc->clip->id);
		return TRUE;
	}
	else if (CTX_data_equals(member, "edit_mask")) {
		if (sc->mask)
			CTX_data_id_pointer_set(result, &sc->mask->id);
		return TRUE;
	}

	return FALSE;
}

/* dropboxes */
static int clip_drop_poll(bContext *UNUSED(C), wmDrag *drag, wmEvent *UNUSED(event))
{
	if (drag->type == WM_DRAG_PATH)
		if (ELEM3(drag->icon, 0, ICON_FILE_IMAGE, ICON_FILE_BLANK)) /* rule might not work? */
			return TRUE;

	return FALSE;
}

static void clip_drop_copy(wmDrag *drag, wmDropBox *drop)
{
	/* copy drag path to properties */
	RNA_string_set(drop->ptr, "filepath", drag->path);
}

/* area+region dropbox definition */
static void clip_dropboxes(void)
{
	ListBase *lb = WM_dropboxmap_find("Clip", SPACE_CLIP, 0);

	WM_dropbox_add(lb, "CLIP_OT_open", clip_drop_poll, clip_drop_copy);
}

static void clip_refresh(const bContext *C, ScrArea *sa)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	wmWindow *window = CTX_wm_window(C);
	Scene *scene = CTX_data_scene(C);
	SpaceClip *sc = (SpaceClip *)sa->spacedata.first;
	ARegion *ar_main = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
	ARegion *ar_tools = BKE_area_find_region_type(sa, RGN_TYPE_TOOLS);
	ARegion *ar_tool_props = BKE_area_find_region_type(sa, RGN_TYPE_TOOL_PROPS);
	ARegion *ar_preview = ED_clip_has_preview_region(C, sa);
	ARegion *ar_properties = ED_clip_has_properties_region(sa);
	ARegion *ar_channels = ED_clip_has_channels_region(sa);
	int main_visible = FALSE, preview_visible = FALSE, tools_visible = FALSE;
	int tool_props_visible = FALSE, properties_visible = FALSE, channels_visible = FALSE;
	int view_changed = FALSE;

	switch (sc->view) {
		case SC_VIEW_CLIP:
			main_visible = TRUE;
			preview_visible = FALSE;
			tools_visible = TRUE;
			tool_props_visible = TRUE;
			properties_visible = TRUE;
			channels_visible = FALSE;
			break;
		case SC_VIEW_GRAPH:
			main_visible = FALSE;
			preview_visible = TRUE;
			tools_visible = FALSE;
			tool_props_visible = FALSE;
			properties_visible = FALSE;
			channels_visible = FALSE;

			reinit_preview_region(C, ar_preview);
			break;
		case SC_VIEW_DOPESHEET:
			main_visible = FALSE;
			preview_visible = TRUE;
			tools_visible = FALSE;
			tool_props_visible = FALSE;
			properties_visible = FALSE;
			channels_visible = TRUE;

			reinit_preview_region(C, ar_preview);
			break;
	}

	if (main_visible) {
		if (ar_main && (ar_main->flag & RGN_FLAG_HIDDEN)) {
			ar_main->flag &= ~RGN_FLAG_HIDDEN;
			ar_main->v2d.flag &= ~V2D_IS_INITIALISED;
			view_changed = TRUE;
		}

		if (ar_main && ar_main->alignment != RGN_ALIGN_NONE) {
			ar_main->alignment = RGN_ALIGN_NONE;
			view_changed = TRUE;
		}
	}
	else {
		if (ar_main && !(ar_main->flag & RGN_FLAG_HIDDEN)) {
			ar_main->flag |= RGN_FLAG_HIDDEN;
			ar_main->v2d.flag &= ~V2D_IS_INITIALISED;
			WM_event_remove_handlers((bContext *)C, &ar_main->handlers);
			view_changed = TRUE;
		}
		if (ar_main && ar_main->alignment != RGN_ALIGN_NONE) {
			ar_main->alignment = RGN_ALIGN_NONE;
			view_changed = TRUE;
		}
	}

	if (properties_visible) {
		if (ar_properties && (ar_properties->flag & RGN_FLAG_HIDDEN)) {
			ar_properties->flag &= ~RGN_FLAG_HIDDEN;
			ar_properties->v2d.flag &= ~V2D_IS_INITIALISED;
			view_changed = TRUE;
		}
		if (ar_properties && ar_properties->alignment != RGN_ALIGN_RIGHT) {
			ar_properties->alignment = RGN_ALIGN_RIGHT;
			view_changed = TRUE;
		}
	}
	else {
		if (ar_properties && !(ar_properties->flag & RGN_FLAG_HIDDEN)) {
			ar_properties->flag |= RGN_FLAG_HIDDEN;
			ar_properties->v2d.flag &= ~V2D_IS_INITIALISED;
			WM_event_remove_handlers((bContext *)C, &ar_properties->handlers);
			view_changed = TRUE;
		}
		if (ar_properties && ar_properties->alignment != RGN_ALIGN_NONE) {
			ar_properties->alignment = RGN_ALIGN_NONE;
			view_changed = TRUE;
		}
	}

	if (tools_visible) {
		if (ar_tools && (ar_tools->flag & RGN_FLAG_HIDDEN)) {
			ar_tools->flag &= ~RGN_FLAG_HIDDEN;
			ar_tools->v2d.flag &= ~V2D_IS_INITIALISED;
			view_changed = TRUE;
		}
		if (ar_tools && ar_tools->alignment != RGN_ALIGN_LEFT) {
			ar_tools->alignment = RGN_ALIGN_LEFT;
			view_changed = TRUE;
		}
	}
	else {
		if (ar_tools && !(ar_tools->flag & RGN_FLAG_HIDDEN)) {
			ar_tools->flag |= RGN_FLAG_HIDDEN;
			ar_tools->v2d.flag &= ~V2D_IS_INITIALISED;
			WM_event_remove_handlers((bContext *)C, &ar_tools->handlers);
			view_changed = TRUE;
		}
		if (ar_tools && ar_tools->alignment != RGN_ALIGN_NONE) {
			ar_tools->alignment = RGN_ALIGN_NONE;
			view_changed = TRUE;
		}
	}

	if (tool_props_visible) {
		if (ar_tool_props && (ar_tool_props->flag & RGN_FLAG_HIDDEN)) {
			ar_tool_props->flag &= ~RGN_FLAG_HIDDEN;
			ar_tool_props->v2d.flag &= ~V2D_IS_INITIALISED;
			view_changed = TRUE;
		}
		if (ar_tool_props && (ar_tool_props->alignment != (RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV))) {
			ar_tool_props->alignment = RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV;
			view_changed = TRUE;
		}
	}
	else {
		if (ar_tool_props && !(ar_tool_props->flag & RGN_FLAG_HIDDEN)) {
			ar_tool_props->flag |= RGN_FLAG_HIDDEN;
			ar_tool_props->v2d.flag &= ~V2D_IS_INITIALISED;
			WM_event_remove_handlers((bContext *)C, &ar_tool_props->handlers);
			view_changed = TRUE;
		}
		if (ar_tool_props && ar_tool_props->alignment != RGN_ALIGN_NONE) {
			ar_tool_props->alignment = RGN_ALIGN_NONE;
			view_changed = TRUE;
		}
	}

	if (preview_visible) {
		if (ar_preview && (ar_preview->flag & RGN_FLAG_HIDDEN)) {
			ar_preview->flag &= ~RGN_FLAG_HIDDEN;
			ar_preview->v2d.flag &= ~V2D_IS_INITIALISED;
			ar_preview->v2d.cur = ar_preview->v2d.tot;
			view_changed = TRUE;
		}
		if (ar_preview && ar_preview->alignment != RGN_ALIGN_NONE) {
			ar_preview->alignment = RGN_ALIGN_NONE;
			view_changed = TRUE;
		}
	}
	else {
		if (ar_preview && !(ar_preview->flag & RGN_FLAG_HIDDEN)) {
			ar_preview->flag |= RGN_FLAG_HIDDEN;
			ar_preview->v2d.flag &= ~V2D_IS_INITIALISED;
			WM_event_remove_handlers((bContext *)C, &ar_preview->handlers);
			view_changed = TRUE;
		}
		if (ar_preview && ar_preview->alignment != RGN_ALIGN_NONE) {
			ar_preview->alignment = RGN_ALIGN_NONE;
			view_changed = TRUE;
		}
	}

	if (channels_visible) {
		if (ar_channels && (ar_channels->flag & RGN_FLAG_HIDDEN)) {
			ar_channels->flag &= ~RGN_FLAG_HIDDEN;
			ar_channels->v2d.flag &= ~V2D_IS_INITIALISED;
			view_changed = TRUE;
		}
		if (ar_channels && ar_channels->alignment != RGN_ALIGN_LEFT) {
			ar_channels->alignment = RGN_ALIGN_LEFT;
			view_changed = TRUE;
		}
	}
	else {
		if (ar_channels && !(ar_channels->flag & RGN_FLAG_HIDDEN)) {
			ar_channels->flag |= RGN_FLAG_HIDDEN;
			ar_channels->v2d.flag &= ~V2D_IS_INITIALISED;
			WM_event_remove_handlers((bContext *)C, &ar_tools->handlers);
			view_changed = TRUE;
		}
		if (ar_channels && ar_channels->alignment != RGN_ALIGN_NONE) {
			ar_channels->alignment = RGN_ALIGN_NONE;
			view_changed = TRUE;
		}
	}

	if (view_changed) {
		ED_area_initialize(wm, window, sa);
		ED_area_tag_redraw(sa);
	}

	BKE_movieclip_user_set_frame(&sc->user, scene->r.cfra);
}

/********************* main region ********************/

/* sets up the fields of the View2D from zoom and offset */
static void movieclip_main_area_set_view2d(SpaceClip *sc, ARegion *ar)
{
	MovieClip *clip = ED_space_clip(sc);
	float x1, y1, w, h;
	int width, height, winx, winy;

	ED_space_clip_size(sc, &width, &height);

	w = width;
	h = height;

	if (clip)
		h *= clip->aspy / clip->aspx / clip->tracking.camera.pixel_aspect;

	winx = ar->winrct.xmax - ar->winrct.xmin + 1;
	winy = ar->winrct.ymax - ar->winrct.ymin + 1;

	ar->v2d.tot.xmin = 0;
	ar->v2d.tot.ymin = 0;
	ar->v2d.tot.xmax = w;
	ar->v2d.tot.ymax = h;

	ar->v2d.mask.xmin = ar->v2d.mask.ymin = 0;
	ar->v2d.mask.xmax = winx;
	ar->v2d.mask.ymax = winy;

	/* which part of the image space do we see? */
	x1 = ar->winrct.xmin + (winx - sc->zoom * w) / 2.0f;
	y1 = ar->winrct.ymin + (winy - sc->zoom * h) / 2.0f;

	x1 -= sc->zoom * sc->xof;
	y1 -= sc->zoom * sc->yof;

	/* relative display right */
	ar->v2d.cur.xmin = (ar->winrct.xmin - (float)x1) / sc->zoom;
	ar->v2d.cur.xmax = ar->v2d.cur.xmin + ((float)winx / sc->zoom);

	/* relative display left */
	ar->v2d.cur.ymin = (ar->winrct.ymin - (float)y1) / sc->zoom;
	ar->v2d.cur.ymax = ar->v2d.cur.ymin + ((float)winy / sc->zoom);

	/* normalize 0.0..1.0 */
	ar->v2d.cur.xmin /= w;
	ar->v2d.cur.xmax /= w;
	ar->v2d.cur.ymin /= h;
	ar->v2d.cur.ymax /= h;
}

/* add handlers, stuff you only do once or on area/region changes */
static void clip_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_STANDARD, ar->winx, ar->winy);

	/* own keymap */
	keymap = WM_keymap_find(wm->defaultconf, "Mask Editing", 0, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

	keymap = WM_keymap_find(wm->defaultconf, "Clip", SPACE_CLIP, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

	keymap = WM_keymap_find(wm->defaultconf, "Clip Editor", SPACE_CLIP, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void clip_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceClip *sc = CTX_wm_space_clip(C);
	Scene *scene = CTX_data_scene(C);
	MovieClip *clip = ED_space_clip(sc);

	/* if tracking is in progress, we should synchronize framenr from clipuser
	 * so latest tracked frame would be shown */
	if (clip && clip->tracking_context)
		BKE_tracking_sync_user(&sc->user, clip->tracking_context);

	if (sc->flag & SC_LOCK_SELECTION) {
		ImBuf *tmpibuf = NULL;

		if (clip && clip->tracking.stabilization.flag & TRACKING_2D_STABILIZATION) {
			tmpibuf = ED_space_clip_get_stable_buffer(sc, NULL, NULL, NULL);
		}

		if (ED_clip_view_selection(sc, ar, 0)) {
			sc->xof += sc->xlockof;
			sc->yof += sc->ylockof;
		}

		if (tmpibuf)
			IMB_freeImBuf(tmpibuf);
	}

	/* clear and setup matrix */
	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);

	/* data... */
	movieclip_main_area_set_view2d(sc, ar);

	clip_draw_main(sc, ar, scene);

	/* Grease Pencil */
	clip_draw_grease_pencil((bContext *)C, 1);

	if (sc->mode == SC_MODE_MASKEDIT) {
		int x, y;
		int width, height;
		float zoomx, zoomy, aspx, aspy;

		/* frame image */
		float maxdim;
		float xofs, yofs;

		/* find window pixel coordinates of origin */
		UI_view2d_to_region_no_clip(&ar->v2d, 0.0f, 0.0f, &x, &y);

		ED_space_clip_size(sc, &width, &height);
		ED_space_clip_zoom(sc, ar, &zoomx, &zoomy);
		ED_space_clip_aspect(sc, &aspx, &aspy);

		/* frame the image */
		maxdim = maxf(width, height);
		if (width == height) {
			xofs = yofs = 0;
		}
		else if (width < height) {
			xofs = ((height - width) / -2.0f) * zoomx;
			yofs = 0.0f;
		}
		else { /* (width > height) */
			xofs = 0.0f;
			yofs = ((width - height) / -2.0f) * zoomy;
		}

		/* apply transformation so mask editing tools will assume drawing from the origin in normalized space */
		glPushMatrix();
		glTranslatef(x + xofs, y + yofs, 0);
		glScalef(maxdim * zoomx, maxdim * zoomy, 0);
		glMultMatrixf(sc->stabmat);

		ED_mask_draw((bContext *)C, sc->mask_draw_flag, sc->mask_draw_type);

		ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);

		glPopMatrix();
	}

	/* reset view matrix */
	UI_view2d_view_restore(C);

	/* draw Grease Pencil - screen space only */
	clip_draw_grease_pencil((bContext *)C, 0);
}

static void clip_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCREEN:
			if (wmn->data == ND_GPENCIL)
				ED_region_tag_redraw(ar);
		break;
	}
}

/****************** preview region ******************/

static void clip_preview_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);

	/* own keymap */
	keymap = WM_keymap_find(wm->defaultconf, "Clip", SPACE_CLIP, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

	keymap = WM_keymap_find(wm->defaultconf, "Clip Graph Editor", SPACE_CLIP, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void graph_area_draw(const bContext *C, ARegion *ar)
{
	View2D *v2d = &ar->v2d;
	View2DScrollers *scrollers;
	SpaceClip *sc = CTX_wm_space_clip(C);
	Scene *scene = CTX_data_scene(C);
	short unitx, unity;

	if (sc->flag & SC_LOCK_TIMECURSOR)
		ED_clip_graph_center_current_frame(scene, ar);

	/* clear and setup matrix */
	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);

	UI_view2d_view_ortho(v2d);

	/* data... */
	clip_draw_graph(sc, ar, scene);

	/* reset view matrix */
	UI_view2d_view_restore(C);

	/* scrollers */
	unitx = (sc->flag & SC_SHOW_SECONDS)? V2D_UNIT_SECONDS : V2D_UNIT_FRAMES;
	unity = V2D_UNIT_VALUES;
	scrollers = UI_view2d_scrollers_calc(C, v2d, unitx, V2D_GRID_NOCLAMP, unity, V2D_GRID_NOCLAMP);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

static void dopesheet_area_draw(const bContext *C, ARegion *ar)
{
	Scene *scene = CTX_data_scene(C);
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	View2D *v2d = &ar->v2d;
	View2DGrid *grid;
	View2DScrollers *scrollers;
	short unit = 0;

	if (clip)
		BKE_tracking_dopesheet_update(&clip->tracking, sc->dope_sort, sc->dope_flag & SC_DOPE_SORT_INVERSE);

	/* clear and setup matrix */
	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);

	UI_view2d_view_ortho(v2d);

	/* time grid */
	unit = (sc->flag & SC_SHOW_SECONDS)? V2D_UNIT_SECONDS : V2D_UNIT_FRAMES;
	grid = UI_view2d_grid_calc(CTX_data_scene(C), v2d, unit, V2D_GRID_CLAMP,
	                           V2D_ARG_DUMMY, V2D_ARG_DUMMY, ar->winx, ar->winy);
	UI_view2d_grid_draw(v2d, grid, V2D_GRIDLINES_ALL);
	UI_view2d_grid_free(grid);

	/* data... */
	clip_draw_dopesheet_main(sc, ar, scene);

	/* reset view matrix */
	UI_view2d_view_restore(C);

	/* scrollers */
	scrollers = UI_view2d_scrollers_calc(C, v2d, unit, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

static void clip_preview_area_draw(const bContext *C, ARegion *ar)
{
	SpaceClip *sc = CTX_wm_space_clip(C);

	if (sc->view == SC_VIEW_GRAPH)
		graph_area_draw(C, ar);
	else if (sc->view == SC_VIEW_DOPESHEET)
		dopesheet_area_draw(C, ar);
}

static void clip_preview_area_listener(ARegion *UNUSED(ar), wmNotifier *UNUSED(wmn))
{
}

/****************** channels region ******************/

static void clip_channels_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);

	keymap = WM_keymap_find(wm->defaultconf, "Clip Dopesheet Editor", SPACE_CLIP, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void clip_channels_area_draw(const bContext *C, ARegion *ar)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	View2D *v2d = &ar->v2d;
	View2DScrollers *scrollers;

	if (clip)
		BKE_tracking_dopesheet_update(&clip->tracking, sc->dope_sort, sc->dope_flag & SC_DOPE_SORT_INVERSE);

	/* clear and setup matrix */
	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);

	UI_view2d_view_ortho(v2d);

	/* data... */
	clip_draw_dopesheet_channels(C, ar);

	/* reset view matrix */
	UI_view2d_view_restore(C);

	/* scrollers */
	scrollers = UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

static void clip_channels_area_listener(ARegion *UNUSED(ar), wmNotifier *UNUSED(wmn))
{
}

/****************** header region ******************/

/* add handlers, stuff you only do once or on area/region changes */
static void clip_header_area_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void clip_header_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void clip_header_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCENE:
			switch (wmn->data) {
				/* for proportional editmode only */
				case ND_TOOLSETTINGS:
					/* TODO - should do this when in mask mode only but no datas available */
					// if (sc->mode == SC_MODE_MASKEDIT)
					{
						ED_region_tag_redraw(ar);
					}
					break;
			}
			break;
	}
}


/****************** tools region ******************/

/* add handlers, stuff you only do once or on area/region changes */
static void clip_tools_area_init(wmWindowManager *wm, ARegion *ar)
{
	ED_region_panels_init(wm, ar);
}

static void clip_tools_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, NULL, -1);
}

/****************** tool properties region ******************/

static void clip_props_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_WM:
			if (wmn->data == ND_HISTORY)
				ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			if (wmn->data == ND_MODE)
				ED_region_tag_redraw(ar);
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_CLIP)
				ED_region_tag_redraw(ar);
			break;
		case NC_SCREEN:
			if (wmn->data == ND_GPENCIL)
				ED_region_tag_redraw(ar);
			break;
	}
}

/****************** properties region ******************/

/* add handlers, stuff you only do once or on area/region changes */
static void clip_properties_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	ED_region_panels_init(wm, ar);

	keymap = WM_keymap_find(wm->defaultconf, "Clip", SPACE_CLIP, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void clip_properties_area_draw(const bContext *C, ARegion *ar)
{
	SpaceClip *sc = CTX_wm_space_clip(C);

	BKE_movieclip_update_scopes(sc->clip, &sc->user, &sc->scopes);

	ED_region_panels(C, ar, 1, NULL, -1);
}

static void clip_properties_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SCREEN:
			if (wmn->data == ND_GPENCIL)
				ED_region_tag_redraw(ar);
			break;
		case NC_BRUSH:
			if (wmn->action == NA_EDITED)
				ED_region_tag_redraw(ar);
			break;
	}
}

/********************* registration ********************/

/* only called once, from space/spacetypes.c */
void ED_spacetype_clip(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype clip");
	ARegionType *art;

	st->spaceid = SPACE_CLIP;
	strncpy(st->name, "Clip", BKE_ST_MAXNAME);

	st->new = clip_new;
	st->free = clip_free;
	st->init = clip_init;
	st->duplicate = clip_duplicate;
	st->operatortypes = clip_operatortypes;
	st->keymap = clip_keymap;
	st->listener = clip_listener;
	st->context = clip_context;
	st->dropboxes = clip_dropboxes;
	st->refresh = clip_refresh;

	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype clip region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init = clip_main_area_init;
	art->draw = clip_main_area_draw;
	art->listener = clip_main_area_listener;
	art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI | ED_KEYMAP_GPENCIL;

	BLI_addhead(&st->regiontypes, art);

	/* preview */
	art = MEM_callocN(sizeof(ARegionType), "spacetype clip region preview");
	art->regionid = RGN_TYPE_PREVIEW;
	art->prefsizey = 240;
	art->init = clip_preview_area_init;
	art->draw = clip_preview_area_draw;
	art->listener = clip_preview_area_listener;
	art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;

	BLI_addhead(&st->regiontypes, art);

	/* regions: properties */
	art = MEM_callocN(sizeof(ARegionType), "spacetype clip region properties");
	art->regionid = RGN_TYPE_UI;
	art->prefsizex = UI_COMPACT_PANEL_WIDTH;
	art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI;
	art->init = clip_properties_area_init;
	art->draw = clip_properties_area_draw;
	art->listener = clip_properties_area_listener;
	BLI_addhead(&st->regiontypes, art);
	ED_clip_buttons_register(art);

	/* regions: tools */
	art = MEM_callocN(sizeof(ARegionType), "spacetype clip region tools");
	art->regionid = RGN_TYPE_TOOLS;
	art->prefsizex = UI_COMPACT_PANEL_WIDTH;
	art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI;
	art->listener = clip_props_area_listener;
	art->init = clip_tools_area_init;
	art->draw = clip_tools_area_draw;

	BLI_addhead(&st->regiontypes, art);

	/* tool properties */
	art = MEM_callocN(sizeof(ARegionType), "spacetype clip tool properties region");
	art->regionid = RGN_TYPE_TOOL_PROPS;
	art->prefsizex = 0;
	art->prefsizey = 120;
	art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI;
	art->listener = clip_props_area_listener;
	art->init = clip_tools_area_init;
	art->draw = clip_tools_area_draw;
	ED_clip_tool_props_register(art);

	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype clip region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

	art->init = clip_header_area_init;
	art->draw = clip_header_area_draw;
	art->listener = clip_header_area_listener;

	BLI_addhead(&st->regiontypes, art);

	BKE_spacetype_register(st);

	/* channels */
	art = MEM_callocN(sizeof(ARegionType), "spacetype clip channels region");
	art->regionid = RGN_TYPE_CHANNELS;
	art->prefsizex = UI_COMPACT_PANEL_WIDTH;
	art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI;
	art->listener = clip_channels_area_listener;
	art->init = clip_channels_area_init;
	art->draw = clip_channels_area_draw;

	BLI_addhead(&st->regiontypes, art);
}
