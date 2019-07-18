/*
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
 */

/** \file
 * \ingroup spclip
 */

#include <string.h>
#include <stdio.h>

#include "DNA_scene_types.h"
#include "DNA_mask_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_view3d_types.h" /* for pivot point */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"
#include "BKE_screen.h"

#include "IMB_imbuf_types.h"

#include "ED_anim_api.h" /* for timeline cursor drawing */
#include "ED_mask.h"
#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_time_scrub_ui.h"
#include "ED_select_utils.h"
#include "ED_clip.h"
#include "ED_transform.h"
#include "ED_uvedit.h" /* just for ED_image_draw_cursor */

#include "IMB_imbuf.h"

#include "GPU_glew.h"
#include "GPU_matrix.h"
#include "GPU_framebuffer.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "clip_intern.h" /* own include */

static void init_preview_region(const Scene *scene,
                                const ScrArea *sa,
                                const SpaceClip *sc,
                                ARegion *ar)
{
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
    ar->v2d.scroll = (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
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

    ar->v2d.scroll = (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
    ar->v2d.scroll |= (V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HANDLES);

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
  Scene *scene = CTX_data_scene(C);
  ScrArea *sa = CTX_wm_area(C);
  SpaceClip *sc = CTX_wm_space_clip(C);

  if (sc->view == SC_VIEW_DOPESHEET) {
    if ((ar->v2d.flag & V2D_VIEWSYNC_AREA_VERTICAL) == 0) {
      init_preview_region(scene, sa, sc, ar);
    }
  }
  else {
    if (ar->v2d.flag & V2D_VIEWSYNC_AREA_VERTICAL) {
      init_preview_region(scene, sa, sc, ar);
    }
  }
}

static ARegion *ED_clip_has_preview_region(const bContext *C, ScrArea *sa)
{
  ARegion *ar, *arnew;

  ar = BKE_area_find_region_type(sa, RGN_TYPE_PREVIEW);
  if (ar) {
    return ar;
  }

  /* add subdiv level; after header */
  ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);

  /* is error! */
  if (ar == NULL) {
    return NULL;
  }

  arnew = MEM_callocN(sizeof(ARegion), "clip preview region");

  BLI_insertlinkbefore(&sa->regionbase, ar, arnew);
  init_preview_region(CTX_data_scene(C), sa, CTX_wm_space_clip(C), arnew);

  return arnew;
}

static ARegion *ED_clip_has_channels_region(ScrArea *sa)
{
  ARegion *ar, *arnew;

  ar = BKE_area_find_region_type(sa, RGN_TYPE_CHANNELS);
  if (ar) {
    return ar;
  }

  /* add subdiv level; after header */
  ar = BKE_area_find_region_type(sa, RGN_TYPE_PREVIEW);

  /* is error! */
  if (ar == NULL) {
    return NULL;
  }

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

  if (sc->mode != SC_MODE_TRACKING) {
    return;
  }

  /* only while properties are visible */
  for (ar = sa->regionbase.first; ar; ar = ar->next) {
    if (ar->regiontype == RGN_TYPE_UI && ar->flag & RGN_FLAG_HIDDEN) {
      return;
    }
  }

  sc->scopes.ok = false;
}

static void clip_scopes_check_gpencil_change(ScrArea *sa)
{
  SpaceClip *sc = (SpaceClip *)sa->spacedata.first;

  if (sc->gpencil_src == SC_GPENCIL_SRC_TRACK) {
    clip_scopes_tag_refresh(sa);
  }
}

static void clip_area_sync_frame_from_scene(ScrArea *sa, Scene *scene)
{
  SpaceClip *space_clip = (SpaceClip *)sa->spacedata.first;
  BKE_movieclip_user_set_frame(&space_clip->user, scene->r.cfra);
}

/* ******************** default callbacks for clip space ***************** */

static SpaceLink *clip_new(const ScrArea *sa, const Scene *scene)
{
  ARegion *ar;
  SpaceClip *sc;

  sc = MEM_callocN(sizeof(SpaceClip), "initclip");
  sc->spacetype = SPACE_CLIP;
  sc->flag = SC_SHOW_MARKER_PATTERN | SC_SHOW_TRACK_PATH | SC_SHOW_GRAPH_TRACKS_MOTION |
             SC_SHOW_GRAPH_FRAMES | SC_SHOW_ANNOTATION;
  sc->zoom = 1.0f;
  sc->path_length = 20;
  sc->scopes.track_preview_height = 120;
  sc->around = V3D_AROUND_CENTER_MEDIAN;

  /* header */
  ar = MEM_callocN(sizeof(ARegion), "header for clip");

  BLI_addtail(&sc->regionbase, ar);
  ar->regiontype = RGN_TYPE_HEADER;
  ar->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* tools view */
  ar = MEM_callocN(sizeof(ARegion), "tools for clip");

  BLI_addtail(&sc->regionbase, ar);
  ar->regiontype = RGN_TYPE_TOOLS;
  ar->alignment = RGN_ALIGN_LEFT;

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
  init_preview_region(scene, sa, sc, ar);

  /* main region */
  ar = MEM_callocN(sizeof(ARegion), "main region for clip");

  BLI_addtail(&sc->regionbase, ar);
  ar->regiontype = RGN_TYPE_WINDOW;

  return (SpaceLink *)sc;
}

/* not spacelink itself */
static void clip_free(SpaceLink *sl)
{
  SpaceClip *sc = (SpaceClip *)sl;

  sc->clip = NULL;

  if (sc->scopes.track_preview) {
    IMB_freeImBuf(sc->scopes.track_preview);
  }

  if (sc->scopes.track_search) {
    IMB_freeImBuf(sc->scopes.track_search);
  }
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
  scn->scopes.track_search = NULL;
  scn->scopes.track_preview = NULL;
  scn->scopes.ok = false;

  return (SpaceLink *)scn;
}

static void clip_listener(wmWindow *UNUSED(win), ScrArea *sa, wmNotifier *wmn, Scene *scene)
{
  /* context changes */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
          clip_scopes_tag_refresh(sa);
          ATTR_FALLTHROUGH;

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
          /* fall-through */

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
      switch (wmn->data) {
        case ND_ANIMPLAY:
          ED_area_tag_redraw(sa);
          break;
        case ND_LAYOUTSET:
          clip_area_sync_frame_from_scene(sa, scene);
          break;
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_CLIP) {
        clip_scopes_tag_refresh(sa);
        ED_area_tag_redraw(sa);
      }
      break;
    case NC_GPENCIL:
      if (wmn->action == NA_EDITED) {
        clip_scopes_check_gpencil_change(sa);
        ED_area_tag_redraw(sa);
      }
      else if (wmn->data & ND_GPENCIL_EDITMODE) {
        ED_area_tag_redraw(sa);
      }
      break;
    case NC_WM:
      switch (wmn->data) {
        case ND_FILEREAD:
        case ND_UNDO:
          clip_area_sync_frame_from_scene(sa, scene);
          break;
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
#ifdef WITH_INPUT_NDOF
  WM_operatortype_append(CLIP_OT_view_ndof);
#endif
  WM_operatortype_append(CLIP_OT_prefetch);
  WM_operatortype_append(CLIP_OT_set_scene_frames);
  WM_operatortype_append(CLIP_OT_cursor_set);

  /* ** tracking_ops.c ** */

  /* navigation */
  WM_operatortype_append(CLIP_OT_frame_jump);

  /* set optical center to frame center */
  WM_operatortype_append(CLIP_OT_set_center_principal);

  /* selection */
  WM_operatortype_append(CLIP_OT_select);
  WM_operatortype_append(CLIP_OT_select_all);
  WM_operatortype_append(CLIP_OT_select_box);
  WM_operatortype_append(CLIP_OT_select_lasso);
  WM_operatortype_append(CLIP_OT_select_circle);
  WM_operatortype_append(CLIP_OT_select_grouped);

  /* markers */
  WM_operatortype_append(CLIP_OT_add_marker);
  WM_operatortype_append(CLIP_OT_add_marker_at_click);
  WM_operatortype_append(CLIP_OT_slide_marker);
  WM_operatortype_append(CLIP_OT_delete_track);
  WM_operatortype_append(CLIP_OT_delete_marker);

  /* track */
  WM_operatortype_append(CLIP_OT_track_markers);
  WM_operatortype_append(CLIP_OT_refine_markers);

  /* solving */
  WM_operatortype_append(CLIP_OT_solve_camera);
  WM_operatortype_append(CLIP_OT_clear_solution);

  WM_operatortype_append(CLIP_OT_disable_markers);
  WM_operatortype_append(CLIP_OT_hide_tracks);
  WM_operatortype_append(CLIP_OT_hide_tracks_clear);
  WM_operatortype_append(CLIP_OT_lock_tracks);

  WM_operatortype_append(CLIP_OT_set_solver_keyframe);

  /* orientation */
  WM_operatortype_append(CLIP_OT_set_origin);
  WM_operatortype_append(CLIP_OT_set_plane);
  WM_operatortype_append(CLIP_OT_set_axis);
  WM_operatortype_append(CLIP_OT_set_scale);
  WM_operatortype_append(CLIP_OT_set_solution_scale);
  WM_operatortype_append(CLIP_OT_apply_solution_scale);

  /* detect */
  WM_operatortype_append(CLIP_OT_detect_features);

  /* stabilization */
  WM_operatortype_append(CLIP_OT_stabilize_2d_add);
  WM_operatortype_append(CLIP_OT_stabilize_2d_remove);
  WM_operatortype_append(CLIP_OT_stabilize_2d_select);
  WM_operatortype_append(CLIP_OT_stabilize_2d_rotation_add);
  WM_operatortype_append(CLIP_OT_stabilize_2d_rotation_remove);
  WM_operatortype_append(CLIP_OT_stabilize_2d_rotation_select);

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

  /* Plane tracker */
  WM_operatortype_append(CLIP_OT_create_plane_track);
  WM_operatortype_append(CLIP_OT_slide_plane_marker);

  WM_operatortype_append(CLIP_OT_keyframe_insert);
  WM_operatortype_append(CLIP_OT_keyframe_delete);

  /* ** clip_graph_ops.c  ** */

  /* graph editing */

  /* selection */
  WM_operatortype_append(CLIP_OT_graph_select);
  WM_operatortype_append(CLIP_OT_graph_select_box);
  WM_operatortype_append(CLIP_OT_graph_select_all_markers);

  WM_operatortype_append(CLIP_OT_graph_delete_curve);
  WM_operatortype_append(CLIP_OT_graph_delete_knot);
  WM_operatortype_append(CLIP_OT_graph_view_all);
  WM_operatortype_append(CLIP_OT_graph_center_current_frame);

  WM_operatortype_append(CLIP_OT_graph_disable_markers);

  /* ** clip_dopesheet_ops.c  ** */

  WM_operatortype_append(CLIP_OT_dopesheet_select_channel);
  WM_operatortype_append(CLIP_OT_dopesheet_view_all);
}

static void clip_keymap(struct wmKeyConfig *keyconf)
{
  /* ******** Global hotkeys avalaible for all regions ******** */
  WM_keymap_ensure(keyconf, "Clip", SPACE_CLIP, 0);

  /* ******** Hotkeys avalaible for main region only ******** */
  WM_keymap_ensure(keyconf, "Clip Editor", SPACE_CLIP, 0);
  //  keymap->poll = ED_space_clip_tracking_poll;

  /* ******** Hotkeys avalaible for preview region only ******** */
  WM_keymap_ensure(keyconf, "Clip Graph Editor", SPACE_CLIP, 0);

  /* ******** Hotkeys avalaible for channels region only ******** */
  WM_keymap_ensure(keyconf, "Clip Dopesheet Editor", SPACE_CLIP, 0);
}

/* DO NOT make this static, this hides the symbol and breaks API generation script. */
extern const char *clip_context_dir[]; /* quiet warning. */
const char *clip_context_dir[] = {"edit_movieclip", "edit_mask", NULL};

static int clip_context(const bContext *C, const char *member, bContextDataResult *result)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, clip_context_dir);

    return true;
  }
  else if (CTX_data_equals(member, "edit_movieclip")) {
    if (sc->clip) {
      CTX_data_id_pointer_set(result, &sc->clip->id);
    }
    return true;
  }
  else if (CTX_data_equals(member, "edit_mask")) {
    if (sc->mask_info.mask) {
      CTX_data_id_pointer_set(result, &sc->mask_info.mask->id);
    }
    return true;
  }

  return false;
}

/* dropboxes */
static bool clip_drop_poll(bContext *UNUSED(C),
                           wmDrag *drag,
                           const wmEvent *UNUSED(event),
                           const char **UNUSED(tooltip))
{
  if (drag->type == WM_DRAG_PATH) {
    /* rule might not work? */
    if (ELEM(drag->icon, 0, ICON_FILE_IMAGE, ICON_FILE_MOVIE, ICON_FILE_BLANK)) {
      return true;
    }
  }

  return false;
}

static void clip_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  PointerRNA itemptr;
  char dir[FILE_MAX], file[FILE_MAX];

  BLI_split_dirfile(drag->path, dir, file, sizeof(dir), sizeof(file));

  RNA_string_set(drop->ptr, "directory", dir);

  RNA_collection_clear(drop->ptr, "files");
  RNA_collection_add(drop->ptr, "files", &itemptr);
  RNA_string_set(&itemptr, "name", file);
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
  ARegion *ar_preview = ED_clip_has_preview_region(C, sa);
  ARegion *ar_properties = ED_clip_has_properties_region(sa);
  ARegion *ar_channels = ED_clip_has_channels_region(sa);
  bool main_visible = false, preview_visible = false, tools_visible = false;
  bool properties_visible = false, channels_visible = false;
  bool view_changed = false;

  switch (sc->view) {
    case SC_VIEW_CLIP:
      main_visible = true;
      preview_visible = false;
      tools_visible = true;
      properties_visible = true;
      channels_visible = false;
      break;
    case SC_VIEW_GRAPH:
      main_visible = false;
      preview_visible = true;
      tools_visible = false;
      properties_visible = false;
      channels_visible = false;

      reinit_preview_region(C, ar_preview);
      break;
    case SC_VIEW_DOPESHEET:
      main_visible = false;
      preview_visible = true;
      tools_visible = false;
      properties_visible = false;
      channels_visible = true;

      reinit_preview_region(C, ar_preview);
      break;
  }

  if (main_visible) {
    if (ar_main && (ar_main->flag & RGN_FLAG_HIDDEN)) {
      ar_main->flag &= ~RGN_FLAG_HIDDEN;
      ar_main->v2d.flag &= ~V2D_IS_INITIALISED;
      view_changed = true;
    }

    if (ar_main && ar_main->alignment != RGN_ALIGN_NONE) {
      ar_main->alignment = RGN_ALIGN_NONE;
      view_changed = true;
    }
  }
  else {
    if (ar_main && !(ar_main->flag & RGN_FLAG_HIDDEN)) {
      ar_main->flag |= RGN_FLAG_HIDDEN;
      ar_main->v2d.flag &= ~V2D_IS_INITIALISED;
      WM_event_remove_handlers((bContext *)C, &ar_main->handlers);
      view_changed = true;
    }
    if (ar_main && ar_main->alignment != RGN_ALIGN_NONE) {
      ar_main->alignment = RGN_ALIGN_NONE;
      view_changed = true;
    }
  }

  if (properties_visible) {
    if (ar_properties && (ar_properties->flag & RGN_FLAG_HIDDEN)) {
      ar_properties->flag &= ~RGN_FLAG_HIDDEN;
      ar_properties->v2d.flag &= ~V2D_IS_INITIALISED;
      view_changed = true;
    }
    if (ar_properties && ar_properties->alignment != RGN_ALIGN_RIGHT) {
      ar_properties->alignment = RGN_ALIGN_RIGHT;
      view_changed = true;
    }
  }
  else {
    if (ar_properties && !(ar_properties->flag & RGN_FLAG_HIDDEN)) {
      ar_properties->flag |= RGN_FLAG_HIDDEN;
      ar_properties->v2d.flag &= ~V2D_IS_INITIALISED;
      WM_event_remove_handlers((bContext *)C, &ar_properties->handlers);
      view_changed = true;
    }
    if (ar_properties && ar_properties->alignment != RGN_ALIGN_NONE) {
      ar_properties->alignment = RGN_ALIGN_NONE;
      view_changed = true;
    }
  }

  if (tools_visible) {
    if (ar_tools && (ar_tools->flag & RGN_FLAG_HIDDEN)) {
      ar_tools->flag &= ~RGN_FLAG_HIDDEN;
      ar_tools->v2d.flag &= ~V2D_IS_INITIALISED;
      view_changed = true;
    }
    if (ar_tools && ar_tools->alignment != RGN_ALIGN_LEFT) {
      ar_tools->alignment = RGN_ALIGN_LEFT;
      view_changed = true;
    }
  }
  else {
    if (ar_tools && !(ar_tools->flag & RGN_FLAG_HIDDEN)) {
      ar_tools->flag |= RGN_FLAG_HIDDEN;
      ar_tools->v2d.flag &= ~V2D_IS_INITIALISED;
      WM_event_remove_handlers((bContext *)C, &ar_tools->handlers);
      view_changed = true;
    }
    if (ar_tools && ar_tools->alignment != RGN_ALIGN_NONE) {
      ar_tools->alignment = RGN_ALIGN_NONE;
      view_changed = true;
    }
  }

  if (preview_visible) {
    if (ar_preview && (ar_preview->flag & RGN_FLAG_HIDDEN)) {
      ar_preview->flag &= ~RGN_FLAG_HIDDEN;
      ar_preview->v2d.flag &= ~V2D_IS_INITIALISED;
      ar_preview->v2d.cur = ar_preview->v2d.tot;
      view_changed = true;
    }
    if (ar_preview && ar_preview->alignment != RGN_ALIGN_NONE) {
      ar_preview->alignment = RGN_ALIGN_NONE;
      view_changed = true;
    }
  }
  else {
    if (ar_preview && !(ar_preview->flag & RGN_FLAG_HIDDEN)) {
      ar_preview->flag |= RGN_FLAG_HIDDEN;
      ar_preview->v2d.flag &= ~V2D_IS_INITIALISED;
      WM_event_remove_handlers((bContext *)C, &ar_preview->handlers);
      view_changed = true;
    }
    if (ar_preview && ar_preview->alignment != RGN_ALIGN_NONE) {
      ar_preview->alignment = RGN_ALIGN_NONE;
      view_changed = true;
    }
  }

  if (channels_visible) {
    if (ar_channels && (ar_channels->flag & RGN_FLAG_HIDDEN)) {
      ar_channels->flag &= ~RGN_FLAG_HIDDEN;
      ar_channels->v2d.flag &= ~V2D_IS_INITIALISED;
      view_changed = true;
    }
    if (ar_channels && ar_channels->alignment != RGN_ALIGN_LEFT) {
      ar_channels->alignment = RGN_ALIGN_LEFT;
      view_changed = true;
    }
  }
  else {
    if (ar_channels && !(ar_channels->flag & RGN_FLAG_HIDDEN)) {
      ar_channels->flag |= RGN_FLAG_HIDDEN;
      ar_channels->v2d.flag &= ~V2D_IS_INITIALISED;
      WM_event_remove_handlers((bContext *)C, &ar_channels->handlers);
      view_changed = true;
    }
    if (ar_channels && ar_channels->alignment != RGN_ALIGN_NONE) {
      ar_channels->alignment = RGN_ALIGN_NONE;
      view_changed = true;
    }
  }

  if (view_changed) {
    ED_area_initialize(wm, window, sa);
    ED_area_tag_redraw(sa);
  }

  BKE_movieclip_user_set_frame(&sc->user, scene->r.cfra);
}

static void CLIP_GGT_navigate(wmGizmoGroupType *gzgt)
{
  VIEW2D_GGT_navigate_impl(gzgt, "CLIP_GGT_navigate");
}

static void clip_gizmos(void)
{
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(
      &(const struct wmGizmoMapType_Params){SPACE_CLIP, RGN_TYPE_WINDOW});

  WM_gizmogrouptype_append_and_link(gzmap_type, CLIP_GGT_navigate);
}

/********************* main region ********************/

/* sets up the fields of the View2D from zoom and offset */
static void movieclip_main_area_set_view2d(const bContext *C, ARegion *ar)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  float x1, y1, w, h, aspx, aspy;
  int width, height, winx, winy;

  ED_space_clip_get_size(sc, &width, &height);
  ED_space_clip_get_aspect(sc, &aspx, &aspy);

  w = width * aspx;
  h = height * aspy;

  winx = BLI_rcti_size_x(&ar->winrct) + 1;
  winy = BLI_rcti_size_y(&ar->winrct) + 1;

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
static void clip_main_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_STANDARD, ar->winx, ar->winy);

  /* mask polls mode */
  keymap = WM_keymap_ensure(wm->defaultconf, "Mask Editing", 0, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "Clip", SPACE_CLIP, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Clip Editor", SPACE_CLIP, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
}

static void clip_main_region_draw(const bContext *C, ARegion *ar)
{
  /* draw entirely, view changes should be handled here */
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  float aspx, aspy, zoomx, zoomy, x, y;
  int width, height;
  bool show_cursor = false;

  /* if tracking is in progress, we should synchronize framenr from clipuser
   * so latest tracked frame would be shown */
  if (clip && clip->tracking_context) {
    BKE_autotrack_context_sync_user(clip->tracking_context, &sc->user);
  }

  if (sc->flag & SC_LOCK_SELECTION) {
    ImBuf *tmpibuf = NULL;

    if (clip && clip->tracking.stabilization.flag & TRACKING_2D_STABILIZATION) {
      tmpibuf = ED_space_clip_get_stable_buffer(sc, NULL, NULL, NULL);
    }

    if (ED_clip_view_selection(C, ar, 0)) {
      sc->xof += sc->xlockof;
      sc->yof += sc->ylockof;
    }

    if (tmpibuf) {
      IMB_freeImBuf(tmpibuf);
    }
  }

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);
  GPU_clear(GPU_COLOR_BIT);

  /* data... */
  movieclip_main_area_set_view2d(C, ar);

  /* callback */
  ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);

  clip_draw_main(C, sc, ar);

  /* TODO(sergey): would be nice to find a way to de-duplicate all this space conversions */
  UI_view2d_view_to_region_fl(&ar->v2d, 0.0f, 0.0f, &x, &y);
  ED_space_clip_get_size(sc, &width, &height);
  ED_space_clip_get_zoom(sc, ar, &zoomx, &zoomy);
  ED_space_clip_get_aspect(sc, &aspx, &aspy);

  if (sc->mode == SC_MODE_MASKEDIT) {
    Mask *mask = CTX_data_edit_mask(C);
    if (mask && clip) {
      ScrArea *sa = CTX_wm_area(C);
      int mask_width, mask_height;
      ED_mask_get_size(sa, &mask_width, &mask_height);
      ED_mask_draw_region(CTX_data_depsgraph(C),
                          mask,
                          ar,
                          sc->mask_info.draw_flag,
                          sc->mask_info.draw_type,
                          sc->mask_info.overlay_mode,
                          mask_width,
                          mask_height,
                          aspx,
                          aspy,
                          true,
                          true,
                          sc->stabmat,
                          C);
    }
  }

  show_cursor |= sc->mode == SC_MODE_MASKEDIT;
  show_cursor |= sc->around == V3D_AROUND_CURSOR;

  if (show_cursor) {
    GPU_matrix_push();
    GPU_matrix_translate_2f(x, y);
    GPU_matrix_scale_2f(zoomx, zoomy);
    GPU_matrix_mul(sc->stabmat);
    GPU_matrix_scale_2f(width, height);
    ED_image_draw_cursor(ar, sc->cursor);
    GPU_matrix_pop();
  }

  clip_draw_cache_and_notes(C, sc, ar);

  if (sc->flag & SC_SHOW_ANNOTATION) {
    /* Grease Pencil */
    clip_draw_grease_pencil((bContext *)C, true);
  }

  /* callback */
  ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  if (sc->flag & SC_SHOW_ANNOTATION) {
    /* draw Grease Pencil - screen space only */
    clip_draw_grease_pencil((bContext *)C, false);
  }

  WM_gizmomap_draw(ar->gizmo_map, C, WM_GIZMOMAP_DRAWSTEP_2D);
}

static void clip_main_region_listener(wmWindow *UNUSED(win),
                                      ScrArea *UNUSED(sa),
                                      ARegion *ar,
                                      wmNotifier *wmn,
                                      const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_GPENCIL:
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(ar);
      }
      else if (wmn->data & ND_GPENCIL_EDITMODE) {
        ED_region_tag_redraw(ar);
      }
      break;
  }
}

/****************** preview region ******************/

static void clip_preview_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);

  /* own keymap */

  keymap = WM_keymap_ensure(wm->defaultconf, "Clip", SPACE_CLIP, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Clip Time Scrub", SPACE_CLIP, RGN_TYPE_PREVIEW);
  WM_event_add_keymap_handler_poll(&ar->handlers, keymap, ED_time_scrub_event_in_region);

  keymap = WM_keymap_ensure(wm->defaultconf, "Clip Graph Editor", SPACE_CLIP, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Clip Dopesheet Editor", SPACE_CLIP, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
}

static void graph_region_draw(const bContext *C, ARegion *ar)
{
  View2D *v2d = &ar->v2d;
  View2DScrollers *scrollers;
  SpaceClip *sc = CTX_wm_space_clip(C);
  Scene *scene = CTX_data_scene(C);
  short cfra_flag = 0;

  if (sc->flag & SC_LOCK_TIMECURSOR) {
    ED_clip_graph_center_current_frame(scene, ar);
  }

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);
  GPU_clear(GPU_COLOR_BIT);

  UI_view2d_view_ortho(v2d);

  /* data... */
  clip_draw_graph(sc, ar, scene);

  /* current frame indicator line */
  if (sc->flag & SC_SHOW_SECONDS) {
    cfra_flag |= DRAWCFRA_UNIT_SECONDS;
  }
  ANIM_draw_cfra(C, v2d, cfra_flag);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* time-scrubbing */
  ED_time_scrub_draw(ar, scene, sc->flag & SC_SHOW_SECONDS, true);

  /* scrollers */
  scrollers = UI_view2d_scrollers_calc(v2d, NULL);
  UI_view2d_scrollers_draw(v2d, scrollers);
  UI_view2d_scrollers_free(scrollers);

  /* scale indicators */
  {
    rcti rect;
    BLI_rcti_init(&rect,
                  0,
                  15 * UI_DPI_FAC,
                  15 * UI_DPI_FAC,
                  UI_DPI_FAC * ar->sizey - UI_TIME_SCRUB_MARGIN_Y);
    UI_view2d_draw_scale_y__values(ar, v2d, &rect, TH_TEXT);
  }
}

static void dopesheet_region_draw(const bContext *C, ARegion *ar)
{
  Scene *scene = CTX_data_scene(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  View2D *v2d = &ar->v2d;
  View2DScrollers *scrollers;
  short cfra_flag = 0;

  if (clip) {
    BKE_tracking_dopesheet_update(&clip->tracking);
  }

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);
  GPU_clear(GPU_COLOR_BIT);

  UI_view2d_view_ortho(v2d);

  /* time grid */
  UI_view2d_draw_lines_x__discrete_frames_or_seconds(v2d, scene, sc->flag & SC_SHOW_SECONDS);

  /* data... */
  clip_draw_dopesheet_main(sc, ar, scene);

  /* current frame indicator line */
  if (sc->flag & SC_SHOW_SECONDS) {
    cfra_flag |= DRAWCFRA_UNIT_SECONDS;
  }
  ANIM_draw_cfra(C, v2d, cfra_flag);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* time-scrubbing */
  ED_time_scrub_draw(ar, scene, sc->flag & SC_SHOW_SECONDS, true);

  /* scrollers */
  scrollers = UI_view2d_scrollers_calc(v2d, NULL);
  UI_view2d_scrollers_draw(v2d, scrollers);
  UI_view2d_scrollers_free(scrollers);
}

static void clip_preview_region_draw(const bContext *C, ARegion *ar)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  if (sc->view == SC_VIEW_GRAPH) {
    graph_region_draw(C, ar);
  }
  else if (sc->view == SC_VIEW_DOPESHEET) {
    dopesheet_region_draw(C, ar);
  }
}

static void clip_preview_region_listener(wmWindow *UNUSED(win),
                                         ScrArea *UNUSED(sa),
                                         ARegion *UNUSED(ar),
                                         wmNotifier *UNUSED(wmn),
                                         const Scene *UNUSED(scene))
{
}

/****************** channels region ******************/

static void clip_channels_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  /* ensure the 2d view sync works - main region has bottom scroller */
  ar->v2d.scroll = V2D_SCROLL_BOTTOM;

  UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);

  keymap = WM_keymap_ensure(wm->defaultconf, "Clip Dopesheet Editor", SPACE_CLIP, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
}

static void clip_channels_region_draw(const bContext *C, ARegion *ar)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  View2D *v2d = &ar->v2d;

  if (clip) {
    BKE_tracking_dopesheet_update(&clip->tracking);
  }

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);
  GPU_clear(GPU_COLOR_BIT);

  UI_view2d_view_ortho(v2d);

  /* data... */
  clip_draw_dopesheet_channels(C, ar);

  /* reset view matrix */
  UI_view2d_view_restore(C);
}

static void clip_channels_region_listener(wmWindow *UNUSED(win),
                                          ScrArea *UNUSED(sa),
                                          ARegion *UNUSED(ar),
                                          wmNotifier *UNUSED(wmn),
                                          const Scene *UNUSED(scene))
{
}

/****************** header region ******************/

/* add handlers, stuff you only do once or on area/region changes */
static void clip_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
  ED_region_header_init(ar);
}

static void clip_header_region_draw(const bContext *C, ARegion *ar)
{
  ED_region_header(C, ar);
}

static void clip_header_region_listener(wmWindow *UNUSED(win),
                                        ScrArea *UNUSED(sa),
                                        ARegion *ar,
                                        wmNotifier *wmn,
                                        const Scene *UNUSED(scene))
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
            break;
          }
      }
      break;
  }
}

/****************** tools region ******************/

/* add handlers, stuff you only do once or on area/region changes */
static void clip_tools_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, ar);

  keymap = WM_keymap_ensure(wm->defaultconf, "Clip", SPACE_CLIP, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void clip_tools_region_draw(const bContext *C, ARegion *ar)
{
  ED_region_panels(C, ar);
}

/****************** tool properties region ******************/

static void clip_props_region_listener(wmWindow *UNUSED(win),
                                       ScrArea *UNUSED(sa),
                                       ARegion *ar,
                                       wmNotifier *wmn,
                                       const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_WM:
      if (wmn->data == ND_HISTORY) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_SCENE:
      if (wmn->data == ND_MODE) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_CLIP) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_GPENCIL:
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(ar);
      }
      break;
  }
}

/****************** properties region ******************/

/* add handlers, stuff you only do once or on area/region changes */
static void clip_properties_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, ar);

  keymap = WM_keymap_ensure(wm->defaultconf, "Clip", SPACE_CLIP, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void clip_properties_region_draw(const bContext *C, ARegion *ar)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  BKE_movieclip_update_scopes(sc->clip, &sc->user, &sc->scopes);

  ED_region_panels(C, ar);
}

static void clip_properties_region_listener(wmWindow *UNUSED(win),
                                            ScrArea *UNUSED(sa),
                                            ARegion *ar,
                                            wmNotifier *wmn,
                                            const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_GPENCIL:
      if (ELEM(wmn->data, ND_DATA, ND_GPENCIL_EDITMODE)) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_BRUSH:
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(ar);
      }
      break;
  }
}

/********************* registration ********************/

static void clip_id_remap(ScrArea *UNUSED(sa), SpaceLink *slink, ID *old_id, ID *new_id)
{
  SpaceClip *sclip = (SpaceClip *)slink;

  if (!ELEM(GS(old_id->name), ID_MC, ID_MSK)) {
    return;
  }

  if ((ID *)sclip->clip == old_id) {
    sclip->clip = (MovieClip *)new_id;
    id_us_ensure_real(new_id);
  }

  if ((ID *)sclip->mask_info.mask == old_id) {
    sclip->mask_info.mask = (Mask *)new_id;
    id_us_ensure_real(new_id);
  }
}

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
  st->gizmos = clip_gizmos;
  st->dropboxes = clip_dropboxes;
  st->refresh = clip_refresh;
  st->id_remap = clip_id_remap;

  /* regions: main window */
  art = MEM_callocN(sizeof(ARegionType), "spacetype clip region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = clip_main_region_init;
  art->draw = clip_main_region_draw;
  art->listener = clip_main_region_listener;
  art->keymapflag = ED_KEYMAP_GIZMO | ED_KEYMAP_FRAMES | ED_KEYMAP_UI | ED_KEYMAP_GPENCIL;

  BLI_addhead(&st->regiontypes, art);

  /* preview */
  art = MEM_callocN(sizeof(ARegionType), "spacetype clip region preview");
  art->regionid = RGN_TYPE_PREVIEW;
  art->prefsizey = 240;
  art->init = clip_preview_region_init;
  art->draw = clip_preview_region_draw;
  art->listener = clip_preview_region_listener;
  art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;

  BLI_addhead(&st->regiontypes, art);

  /* regions: properties */
  art = MEM_callocN(sizeof(ARegionType), "spacetype clip region properties");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI;
  art->init = clip_properties_region_init;
  art->draw = clip_properties_region_draw;
  art->listener = clip_properties_region_listener;
  BLI_addhead(&st->regiontypes, art);
  ED_clip_buttons_register(art);

  /* regions: tools */
  art = MEM_callocN(sizeof(ARegionType), "spacetype clip region tools");
  art->regionid = RGN_TYPE_TOOLS;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI;
  art->listener = clip_props_region_listener;
  art->init = clip_tools_region_init;
  art->draw = clip_tools_region_draw;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN(sizeof(ARegionType), "spacetype clip region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

  art->init = clip_header_region_init;
  art->draw = clip_header_region_draw;
  art->listener = clip_header_region_listener;

  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);

  /* channels */
  art = MEM_callocN(sizeof(ARegionType), "spacetype clip channels region");
  art->regionid = RGN_TYPE_CHANNELS;
  art->prefsizex = UI_COMPACT_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI;
  art->listener = clip_channels_region_listener;
  art->init = clip_channels_region_init;
  art->draw = clip_channels_region_draw;

  BLI_addhead(&st->regiontypes, art);

  /* regions: hud */
  art = ED_area_type_hud(st->spaceid);
  BLI_addhead(&st->regiontypes, art);
}
