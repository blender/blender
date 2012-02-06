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
#include "DNA_movieclip_types.h"

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
	Scene *scene= CTX_data_scene(C);

	ar->regiontype= RGN_TYPE_PREVIEW;
	ar->alignment= RGN_ALIGN_TOP;
	ar->flag|= RGN_FLAG_HIDDEN;

	ar->v2d.tot.xmin= 0.0f;
	ar->v2d.tot.ymin= -10.0f;
	ar->v2d.tot.xmax= (float)scene->r.efra;
	ar->v2d.tot.ymax= 10.0f;

	ar->v2d.cur= ar->v2d.tot;

	ar->v2d.min[0]= FLT_MIN;
	ar->v2d.min[1]= FLT_MIN;

	ar->v2d.max[0]= MAXFRAMEF;
	ar->v2d.max[1]= FLT_MAX;

	ar->v2d.scroll= (V2D_SCROLL_BOTTOM|V2D_SCROLL_SCALE_HORIZONTAL);
	ar->v2d.scroll |= (V2D_SCROLL_LEFT|V2D_SCROLL_SCALE_VERTICAL);

	ar->v2d.keeptot= 0;
}

static ARegion *clip_has_preview_region(const bContext *C, ScrArea *sa)
{
	ARegion *ar, *arnew;

	ar= BKE_area_find_region_type(sa, RGN_TYPE_PREVIEW);
	if(ar)
		return ar;

	/* add subdiv level; after header */
	ar= BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);

	/* is error! */
	if(ar==NULL)
		return NULL;

	arnew= MEM_callocN(sizeof(ARegion), "clip preview region");

	BLI_insertlinkbefore(&sa->regionbase, ar, arnew);
	init_preview_region(C, arnew);

	return arnew;
}

static void clip_scopes_tag_refresh(ScrArea *sa)
{
	SpaceClip *sc= (SpaceClip *)sa->spacedata.first;
	ARegion *ar;

	if(sc->mode!=SC_MODE_TRACKING)
		return;

	/* only while proeprties are visible */
	for (ar=sa->regionbase.first; ar; ar=ar->next) {
		if (ar->regiontype == RGN_TYPE_UI && ar->flag & RGN_FLAG_HIDDEN)
			return;
	}

	sc->scopes.ok= 0;
}

static void clip_stabilization_tag_refresh(ScrArea *sa)
{
	SpaceClip *sc= (SpaceClip *)sa->spacedata.first;
	MovieClip *clip= ED_space_clip(sc);

	if(clip) {
		MovieTrackingStabilization *stab= &clip->tracking.stabilization;

		stab->ok= 0;
	}
}

/* ******************** default callbacks for clip space ***************** */

static SpaceLink *clip_new(const bContext *C)
{
	ARegion *ar;
	SpaceClip *sc;

	sc= MEM_callocN(sizeof(SpaceClip), "initclip");
	sc->spacetype= SPACE_CLIP;
	sc->flag= SC_SHOW_MARKER_PATTERN|SC_SHOW_TRACK_PATH|SC_MANUAL_CALIBRATION|SC_SHOW_GRAPH_TRACKS|SC_SHOW_GRAPH_FRAMES;
	sc->zoom= 1.0f;
	sc->path_length= 20;
	sc->scopes.track_preview_height= 120;

	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for clip");

	BLI_addtail(&sc->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;

	/* tools view */
	ar= MEM_callocN(sizeof(ARegion), "tools for clip");

	BLI_addtail(&sc->regionbase, ar);
	ar->regiontype= RGN_TYPE_TOOLS;
	ar->alignment= RGN_ALIGN_LEFT;

	/* tool properties */
	ar= MEM_callocN(sizeof(ARegion), "tool properties for clip");

	BLI_addtail(&sc->regionbase, ar);
	ar->regiontype= RGN_TYPE_TOOL_PROPS;
	ar->alignment= RGN_ALIGN_BOTTOM|RGN_SPLIT_PREV;

	/* properties view */
	ar= MEM_callocN(sizeof(ARegion), "properties for clip");

	BLI_addtail(&sc->regionbase, ar);
	ar->regiontype= RGN_TYPE_UI;
	ar->alignment= RGN_ALIGN_RIGHT;

	/* preview view */
	ar= MEM_callocN(sizeof(ARegion), "preview for clip");

	BLI_addtail(&sc->regionbase, ar);
	init_preview_region(C, ar);

	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for clip");

	BLI_addtail(&sc->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;

	return (SpaceLink *)sc;
}

/* not spacelink itself */
static void clip_free(SpaceLink *sl)
{
	SpaceClip *sc= (SpaceClip*) sl;

	sc->clip= NULL;

	if(sc->scopes.track_preview)
		IMB_freeImBuf(sc->scopes.track_preview);
}

/* spacetype; init callback */
static void clip_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{

}

static SpaceLink *clip_duplicate(SpaceLink *sl)
{
	SpaceClip *scn= MEM_dupallocN(sl);

	/* clear or remove stuff from old */
	scn->scopes.track_preview= NULL;
	scn->scopes.ok= 0;

	return (SpaceLink *)scn;
}

static void clip_listener(ScrArea *sa, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_SCENE:
			switch(wmn->data) {
				case ND_FRAME:
					clip_scopes_tag_refresh(sa);
					/* no break! */

				case ND_FRAME_RANGE:
					ED_area_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		case NC_MOVIECLIP:
			switch(wmn->data) {
				case ND_DISPLAY:
				case ND_SELECT:
					clip_scopes_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
			}
			switch(wmn->action) {
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
		case NC_GEOM:
			switch(wmn->data) {
				case ND_SELECT:
					clip_scopes_tag_refresh(sa);
					ED_area_tag_redraw(sa);
					break;
			}
			break;
		 case NC_SCREEN:
			if(wmn->data==ND_ANIMPLAY) {
				ED_area_tag_redraw(sa);
			}
			break;
		case NC_SPACE:
			if(wmn->data==ND_SPACE_CLIP) {
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
	WM_operatortype_append(CLIP_OT_set_floor);
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

	/* graph editing */
	WM_operatortype_append(CLIP_OT_graph_select);
	WM_operatortype_append(CLIP_OT_graph_delete_curve);
	WM_operatortype_append(CLIP_OT_graph_delete_knot);
	WM_operatortype_append(CLIP_OT_graph_view_all);
	WM_operatortype_append(CLIP_OT_graph_center_current_frame);

	/* object tracking */
	WM_operatortype_append(CLIP_OT_tracking_object_new);
	WM_operatortype_append(CLIP_OT_tracking_object_remove);

	/* clipboard */
	WM_operatortype_append(CLIP_OT_copy_tracks);
	WM_operatortype_append(CLIP_OT_paste_tracks);
}

static void clip_keymap(struct wmKeyConfig *keyconf)
{
	wmKeyMap *keymap;
	wmKeyMapItem *kmi;

	/* ******** Global hotkeys avalaible for all regions ******** */

	keymap= WM_keymap_find(keyconf, "Clip", SPACE_CLIP, 0);

	WM_keymap_add_item(keymap, "CLIP_OT_open", OKEY, KM_PRESS, KM_ALT, 0);

	WM_keymap_add_item(keymap, "CLIP_OT_tools", TKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_properties", NKEY, KM_PRESS, 0, 0);

	/* 2d tracking */
	kmi= WM_keymap_add_item(keymap, "CLIP_OT_track_markers", LEFTARROWKEY, KM_PRESS, KM_ALT, 0);
	RNA_boolean_set(kmi->ptr, "backwards", TRUE);
	RNA_boolean_set(kmi->ptr, "sequence", FALSE);
	WM_keymap_add_item(keymap, "CLIP_OT_track_markers", RIGHTARROWKEY, KM_PRESS, KM_ALT, 0);
	kmi= WM_keymap_add_item(keymap, "CLIP_OT_track_markers", TKEY, KM_PRESS, KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "backwards", FALSE);
	RNA_boolean_set(kmi->ptr, "sequence", TRUE);
	kmi= WM_keymap_add_item(keymap, "CLIP_OT_track_markers", TKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0);
	RNA_boolean_set(kmi->ptr, "backwards", TRUE);
	RNA_boolean_set(kmi->ptr, "sequence", TRUE);

	/* mode */
	kmi= WM_keymap_add_item(keymap, "CLIP_OT_mode_set", TABKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "mode", SC_MODE_RECONSTRUCTION);
	RNA_boolean_set(kmi->ptr, "toggle", TRUE);

	kmi= WM_keymap_add_item(keymap, "CLIP_OT_mode_set", TABKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "mode", SC_MODE_DISTORTION);
	RNA_boolean_set(kmi->ptr, "toggle", TRUE);

	kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle_enum", ZKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.view");
	RNA_string_set(kmi->ptr, "value_1", "CLIP");
	RNA_string_set(kmi->ptr, "value_2", "GRAPH");

	WM_keymap_add_item(keymap, "CLIP_OT_solve_camera", SKEY, KM_PRESS, KM_SHIFT, 0);

	/* ******** Hotkeys avalaible for main region only ******** */

	keymap= WM_keymap_find(keyconf, "Clip Editor", SPACE_CLIP, 0);

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
	WM_keymap_add_item(keymap, "CLIP_OT_view_selected", PADPERIOD, KM_PRESS, 0, 0);

	/* jump to special frame */
	kmi= WM_keymap_add_item(keymap, "CLIP_OT_frame_jump", LEFTARROWKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "position", 0);

	kmi= WM_keymap_add_item(keymap, "CLIP_OT_frame_jump", RIGHTARROWKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "position", 1);

	kmi= WM_keymap_add_item(keymap, "CLIP_OT_frame_jump", LEFTARROWKEY, KM_PRESS, KM_ALT|KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "position", 2);

	kmi= WM_keymap_add_item(keymap, "CLIP_OT_frame_jump", RIGHTARROWKEY, KM_PRESS, KM_ALT|KM_SHIFT, 0);
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

	/* marker */
	WM_keymap_add_item(keymap, "CLIP_OT_add_marker_slide", LEFTMOUSE, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "CLIP_OT_delete_marker", DELKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_delete_marker", XKEY, KM_PRESS, KM_SHIFT, 0);

	WM_keymap_add_item(keymap, "CLIP_OT_slide_marker", LEFTMOUSE, KM_PRESS, 0, 0);

	kmi= WM_keymap_add_item(keymap, "CLIP_OT_disable_markers", DKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "action", 2);	/* toggle */

	/* tracks */
	WM_keymap_add_item(keymap, "CLIP_OT_delete_track", DELKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_delete_track", XKEY, KM_PRESS, 0, 0);

	kmi= WM_keymap_add_item(keymap, "CLIP_OT_lock_tracks", LKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", 0);	/* lock */

	kmi= WM_keymap_add_item(keymap, "CLIP_OT_lock_tracks", LKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "action", 1);	/* unlock */

	kmi= WM_keymap_add_item(keymap, "CLIP_OT_hide_tracks", HKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "unselected", FALSE);

	kmi= WM_keymap_add_item(keymap, "CLIP_OT_hide_tracks", HKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "unselected", TRUE);

	WM_keymap_add_item(keymap, "CLIP_OT_hide_tracks_clear", HKEY, KM_PRESS, KM_ALT, 0);

	/* clean-up */
	kmi= WM_keymap_add_item(keymap, "CLIP_OT_clear_track_path", TKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "action", TRACK_CLEAR_REMAINED);
	kmi= WM_keymap_add_item(keymap, "CLIP_OT_clear_track_path", TKEY, KM_PRESS, KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "action", TRACK_CLEAR_UPTO);
	kmi= WM_keymap_add_item(keymap, "CLIP_OT_clear_track_path", TKEY, KM_PRESS, KM_ALT|KM_SHIFT, 0);
	RNA_enum_set(kmi->ptr, "action", TRACK_CLEAR_ALL);

	WM_keymap_add_item(keymap, "CLIP_OT_join_tracks", JKEY, KM_PRESS, KM_CTRL, 0);

	/* menus */
	WM_keymap_add_menu(keymap, "CLIP_MT_tracking_specials", WKEY, KM_PRESS, 0, 0);

	/* display */
	kmi= WM_keymap_add_item(keymap, "WM_OT_context_toggle", LKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.lock_selection");

	kmi= WM_keymap_add_item(keymap, "WM_OT_context_toggle", DKEY, KM_PRESS, KM_ALT, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.show_disabled");

	kmi= WM_keymap_add_item(keymap, "WM_OT_context_toggle", SKEY, KM_PRESS, KM_ALT, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.show_marker_search");

	kmi= WM_keymap_add_item(keymap, "WM_OT_context_toggle", MKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.use_mute_footage");

	transform_keymap_for_space(keyconf, keymap, SPACE_CLIP);

	/* ******** Hotkeys avalaible for preview region only ******** */

	keymap= WM_keymap_find(keyconf, "Clip Graph Editor", SPACE_CLIP, 0);

	/* "timeline" */
	WM_keymap_add_item(keymap, "CLIP_OT_change_frame", ACTIONMOUSE, KM_PRESS, 0, 0);

	/* selection */
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_graph_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", FALSE);
	kmi = WM_keymap_add_item(keymap, "CLIP_OT_graph_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", TRUE);

	/* delete */
	WM_keymap_add_item(keymap, "CLIP_OT_graph_delete_curve", DELKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_graph_delete_curve", XKEY, KM_PRESS, 0, 0);

	WM_keymap_add_item(keymap, "CLIP_OT_graph_delete_knot", DELKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_graph_delete_knot", XKEY, KM_PRESS, KM_SHIFT, 0);

	/* view */
	WM_keymap_add_item(keymap, "CLIP_OT_graph_view_all", HOMEKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "CLIP_OT_graph_center_current_frame", PADPERIOD, KM_PRESS, 0, 0);

	kmi= WM_keymap_add_item(keymap, "WM_OT_context_toggle", LKEY, KM_PRESS, 0, 0);
	RNA_string_set(kmi->ptr, "data_path", "space_data.lock_time_cursor");

	transform_keymap_for_space(keyconf, keymap, SPACE_CLIP);
}

const char *clip_context_dir[]= {"edit_movieclip", NULL};

static int clip_context(const bContext *C, const char *member, bContextDataResult *result)
{
	SpaceClip *sc= CTX_wm_space_clip(C);

	if(CTX_data_dir(member)) {
		CTX_data_dir_set(result, clip_context_dir);
		return 1;
	}
	else if(CTX_data_equals(member, "edit_movieclip")) {
		CTX_data_id_pointer_set(result, &sc->clip->id);
		return 1;
	}

	return 0;
}

static void clip_refresh(const bContext *C, ScrArea *sa)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	wmWindow *window= CTX_wm_window(C);
	Scene *scene = CTX_data_scene(C);
	SpaceClip *sc= (SpaceClip *)sa->spacedata.first;
	ARegion *ar_main= BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
	ARegion *ar_preview= clip_has_preview_region(C, sa);
	int view_changed= 0;

	switch (sc->view) {
		case SC_VIEW_CLIP:
			if (ar_preview && !(ar_preview->flag & RGN_FLAG_HIDDEN)) {
				ar_preview->flag |= RGN_FLAG_HIDDEN;
				ar_preview->v2d.flag &= ~V2D_IS_INITIALISED;
				WM_event_remove_handlers((bContext*)C, &ar_preview->handlers);
				view_changed= 1;
			}
			if (ar_main && ar_main->alignment != RGN_ALIGN_NONE) {
				ar_main->alignment= RGN_ALIGN_NONE;
				view_changed= 1;
			}
			break;
		case SC_VIEW_GRAPH:
			if (ar_preview && (ar_preview->flag & RGN_FLAG_HIDDEN)) {
				ar_preview->flag &= ~RGN_FLAG_HIDDEN;
				ar_preview->v2d.flag &= ~V2D_IS_INITIALISED;
				ar_preview->v2d.cur = ar_preview->v2d.tot;
				view_changed= 1;
			}
			if (ar_main && ar_main->alignment != RGN_ALIGN_NONE) {
				ar_main->alignment= RGN_ALIGN_NONE;
				view_changed= 1;
			}
			if (ar_preview && !ELEM(ar_preview->alignment, RGN_ALIGN_TOP,  RGN_ALIGN_BOTTOM)) {
				ar_preview->alignment= RGN_ALIGN_TOP;
				view_changed= 1;
			}
			break;
	}

	if(view_changed) {
		ED_area_initialize(wm, window, sa);
		ED_area_tag_redraw(sa);
	}

	BKE_movieclip_user_set_frame(&sc->user, scene->r.cfra);
}

/********************* main region ********************/

/* sets up the fields of the View2D from zoom and offset */
static void movieclip_main_area_set_view2d(SpaceClip *sc, ARegion *ar)
{
	MovieClip *clip= ED_space_clip(sc);
	float x1, y1, w, h;
	int width, height, winx, winy;

	ED_space_clip_size(sc, &width, &height);

	w= width;
	h= height;

	if(clip)
		h*= clip->aspy/clip->aspx/clip->tracking.camera.pixel_aspect;

	winx= ar->winrct.xmax - ar->winrct.xmin + 1;
	winy= ar->winrct.ymax - ar->winrct.ymin + 1;

	ar->v2d.tot.xmin= 0;
	ar->v2d.tot.ymin= 0;
	ar->v2d.tot.xmax= w;
	ar->v2d.tot.ymax= h;

	ar->v2d.mask.xmin= ar->v2d.mask.ymin= 0;
	ar->v2d.mask.xmax= winx;
	ar->v2d.mask.ymax= winy;

	/* which part of the image space do we see? */
	x1= ar->winrct.xmin+(winx-sc->zoom*w)/2.0f;
	y1= ar->winrct.ymin+(winy-sc->zoom*h)/2.0f;

	x1-= sc->zoom*sc->xof;
	y1-= sc->zoom*sc->yof;

	/* relative display right */
	ar->v2d.cur.xmin= ((ar->winrct.xmin - (float)x1)/sc->zoom);
	ar->v2d.cur.xmax= ar->v2d.cur.xmin + ((float)winx/sc->zoom);

	/* relative display left */
	ar->v2d.cur.ymin= ((ar->winrct.ymin-(float)y1)/sc->zoom);
	ar->v2d.cur.ymax= ar->v2d.cur.ymin + ((float)winy/sc->zoom);

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
	keymap= WM_keymap_find(wm->defaultconf, "Clip", SPACE_CLIP, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

	keymap= WM_keymap_find(wm->defaultconf, "Clip Editor", SPACE_CLIP, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void clip_main_area_draw(const bContext *C, ARegion *ar)
{
	/* draw entirely, view changes should be handled here */
	SpaceClip *sc= CTX_wm_space_clip(C);
	Scene *scene= CTX_data_scene(C);
	MovieClip *clip= ED_space_clip(sc);

	/* if trcking is in progress, we should sunchronize framenr from clipuser
	   so latest tracked frame would be shown */
	if(clip && clip->tracking_context)
		BKE_tracking_sync_user(&sc->user, clip->tracking_context);

	if(sc->flag&SC_LOCK_SELECTION) {
		ImBuf *tmpibuf= NULL;

		if(clip && clip->tracking.stabilization.flag&TRACKING_2D_STABILIZATION) {
			tmpibuf= ED_space_clip_get_stable_buffer(sc, NULL, NULL, NULL);
		}

		if(ED_clip_view_selection(sc, ar, 0)) {
			sc->xof+= sc->xlockof;
			sc->yof+= sc->ylockof;
		}

		if(tmpibuf)
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

	/* reset view matrix */
	UI_view2d_view_restore(C);

	/* draw Grease Pencil - screen space only */
	clip_draw_grease_pencil((bContext *)C, 0);
}

static void clip_main_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_SCREEN:
			if (wmn->data==ND_GPENCIL)
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
	keymap= WM_keymap_find(wm->defaultconf, "Clip", SPACE_CLIP, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

	keymap= WM_keymap_find(wm->defaultconf, "Clip Graph Editor", SPACE_CLIP, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void clip_preview_area_draw(const bContext *C, ARegion *ar)
{
	View2D *v2d= &ar->v2d;
	View2DScrollers *scrollers;
	SpaceClip *sc= CTX_wm_space_clip(C);
	Scene *scene= CTX_data_scene(C);
	short unitx= V2D_UNIT_FRAMESCALE, unity= V2D_UNIT_VALUES;

	if(sc->flag & SC_LOCK_TIMECURSOR)
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
	scrollers= UI_view2d_scrollers_calc(C, v2d, unitx, V2D_GRID_NOCLAMP, unity, V2D_GRID_NOCLAMP);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}

static void clip_preview_area_listener(ARegion *UNUSED(ar), wmNotifier *UNUSED(wmn))
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
	switch(wmn->category) {
		case NC_WM:
			if(wmn->data == ND_HISTORY)
				ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
			if(wmn->data == ND_MODE)
				ED_region_tag_redraw(ar);
			break;
		case NC_SPACE:
			if(wmn->data == ND_SPACE_CLIP)
				ED_region_tag_redraw(ar);
			break;
		case NC_SCREEN:
			if(wmn->data == ND_GPENCIL)
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

	keymap= WM_keymap_find(wm->defaultconf, "Clip", SPACE_CLIP, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void clip_properties_area_draw(const bContext *C, ARegion *ar)
{
	SpaceClip *sc= CTX_wm_space_clip(C);

	BKE_movieclip_update_scopes(sc->clip, &sc->user, &sc->scopes);

	ED_region_panels(C, ar, 1, NULL, -1);
}

static void clip_properties_area_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_SCREEN:
			if (wmn->data==ND_GPENCIL)
				ED_region_tag_redraw(ar);
			break;
		case NC_BRUSH:
			if(wmn->action==NA_EDITED)
				ED_region_tag_redraw(ar);
			break;
	}
}

/********************* registration ********************/

/* only called once, from space/spacetypes.c */
void ED_spacetype_clip(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype clip");
	ARegionType *art;

	st->spaceid= SPACE_CLIP;
	strncpy(st->name, "Clip", BKE_ST_MAXNAME);

	st->new= clip_new;
	st->free= clip_free;
	st->init= clip_init;
	st->duplicate= clip_duplicate;
	st->operatortypes= clip_operatortypes;
	st->keymap= clip_keymap;
	st->listener= clip_listener;
	st->context= clip_context;
	st->refresh= clip_refresh;

	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype clip region");
	art->regionid= RGN_TYPE_WINDOW;
	art->init= clip_main_area_init;
	art->draw= clip_main_area_draw;
	art->listener= clip_main_area_listener;
	art->keymapflag= ED_KEYMAP_FRAMES|ED_KEYMAP_UI|ED_KEYMAP_GPENCIL;

	BLI_addhead(&st->regiontypes, art);

	/* preview */
	art= MEM_callocN(sizeof(ARegionType), "spacetype clip region preview");
	art->regionid = RGN_TYPE_PREVIEW;
	art->prefsizey = 240;
	art->init= clip_preview_area_init;
	art->draw= clip_preview_area_draw;
	art->listener= clip_preview_area_listener;
	art->keymapflag= ED_KEYMAP_FRAMES|ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;

	BLI_addhead(&st->regiontypes, art);

	/* regions: properties */
	art= MEM_callocN(sizeof(ARegionType), "spacetype clip region properties");
	art->regionid= RGN_TYPE_UI;
	art->prefsizex= UI_COMPACT_PANEL_WIDTH;
	art->keymapflag= ED_KEYMAP_FRAMES|ED_KEYMAP_UI;
	art->init= clip_properties_area_init;
	art->draw= clip_properties_area_draw;
	art->listener= clip_properties_area_listener;
	BLI_addhead(&st->regiontypes, art);
	ED_clip_buttons_register(art);

	/* regions: tools */
	art= MEM_callocN(sizeof(ARegionType), "spacetype clip region tools");
	art->regionid= RGN_TYPE_TOOLS;
	art->prefsizex= UI_COMPACT_PANEL_WIDTH;
	art->keymapflag= ED_KEYMAP_FRAMES|ED_KEYMAP_UI;
	art->listener= clip_props_area_listener;
	art->init= clip_tools_area_init;
	art->draw= clip_tools_area_draw;

	BLI_addhead(&st->regiontypes, art);

	/* tool properties */
	art= MEM_callocN(sizeof(ARegionType), "spacetype clip tool properties region");
	art->regionid = RGN_TYPE_TOOL_PROPS;
	art->prefsizex= 0;
	art->prefsizey= 120;
	art->keymapflag= ED_KEYMAP_FRAMES|ED_KEYMAP_UI;
	art->listener= clip_props_area_listener;
	art->init= clip_tools_area_init;
	art->draw= clip_tools_area_draw;
	ED_clip_tool_props_register(art);

	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype clip region");
	art->regionid= RGN_TYPE_HEADER;
	art->prefsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_FRAMES|ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_HEADER;

	art->init= clip_header_area_init;
	art->draw= clip_header_area_draw;

	BLI_addhead(&st->regiontypes, art);

	BKE_spacetype_register(st);
}
