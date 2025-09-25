/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include <cfloat>
#include <cstring>

#include "DNA_defaults.h"

#include "DNA_mask_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h" /* for pivot point */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_path_utils.hh"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_movieclip.h"
#include "BKE_screen.hh"
#include "BKE_tracking.h"

#include "IMB_imbuf_types.hh"

#include "ED_anim_api.hh" /* for timeline cursor drawing */
#include "ED_clip.hh"
#include "ED_mask.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_time_scrub_ui.hh"
#include "ED_uvedit.hh" /* just for ED_image_draw_cursor */

#include "IMB_imbuf.hh"

#include "GPU_matrix.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "BLO_read_write.hh"

#include "RNA_access.hh"

#include "clip_intern.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

static void init_preview_region(const Scene *scene,
                                const ScrArea *area,
                                const SpaceClip *sc,
                                ARegion *region)
{
  if (sc->view == SC_VIEW_DOPESHEET) {
    region->v2d.tot.xmin = -10.0f;
    region->v2d.tot.ymin = float(-area->winy) / 3.0f;
    region->v2d.tot.xmax = float(area->winx);
    region->v2d.tot.ymax = 0.0f;

    region->v2d.cur = region->v2d.tot;

    region->v2d.min[0] = 0.0f;
    region->v2d.min[1] = 0.0f;

    region->v2d.max[0] = MAXFRAMEF;
    region->v2d.max[1] = FLT_MAX;

    region->v2d.minzoom = 0.01f;
    region->v2d.maxzoom = 50;
    region->v2d.scroll = (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
    region->v2d.scroll |= V2D_SCROLL_RIGHT;
    region->v2d.keepzoom = V2D_LOCKZOOM_Y;
    region->v2d.keepofs = V2D_KEEPOFS_Y;
    region->v2d.align = V2D_ALIGN_NO_POS_Y;
    region->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;
  }
  else {
    region->v2d.tot.xmin = 0.0f;
    region->v2d.tot.ymin = -10.0f;
    region->v2d.tot.xmax = float(scene->r.efra);
    region->v2d.tot.ymax = 10.0f;

    region->v2d.cur = region->v2d.tot;

    region->v2d.min[0] = FLT_MIN;
    region->v2d.min[1] = FLT_MIN;

    region->v2d.max[0] = MAXFRAMEF;
    region->v2d.max[1] = FLT_MAX;

    region->v2d.scroll = (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
    region->v2d.scroll |= (V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HANDLES);

    region->v2d.minzoom = 0.0f;
    region->v2d.maxzoom = 0.0f;
    region->v2d.keepzoom = 0;
    region->v2d.keepofs = 0;
    region->v2d.align = 0;
    region->v2d.flag = 0;

    region->v2d.keeptot = 0;
  }
}

static void clip_scopes_tag_refresh(ScrArea *area)
{
  SpaceClip *sc = (SpaceClip *)area->spacedata.first;

  if (sc->mode != SC_MODE_TRACKING) {
    return;
  }

  /* only while properties are visible */
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_UI && region->flag & RGN_FLAG_HIDDEN) {
      return;
    }
  }

  sc->scopes.ok = false;
}

static void clip_scopes_check_gpencil_change(ScrArea *area)
{
  SpaceClip *sc = (SpaceClip *)area->spacedata.first;

  if (sc->gpencil_src == SC_GPENCIL_SRC_TRACK) {
    clip_scopes_tag_refresh(area);
  }
}

static void clip_area_sync_frame_from_scene(ScrArea *area, const Scene *scene)
{
  SpaceClip *space_clip = (SpaceClip *)area->spacedata.first;
  BKE_movieclip_user_set_frame(&space_clip->user, scene->r.cfra);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Default Callbacks for Clip Space
 * \{ */

static SpaceLink *clip_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  ARegion *region;
  SpaceClip *sc;

  sc = DNA_struct_default_alloc(SpaceClip);

  /* header */
  region = BKE_area_region_new();

  BLI_addtail(&sc->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* tools view */
  region = BKE_area_region_new();

  BLI_addtail(&sc->regionbase, region);
  region->regiontype = RGN_TYPE_TOOLS;
  region->alignment = RGN_ALIGN_LEFT;

  /* properties view */
  region = BKE_area_region_new();

  BLI_addtail(&sc->regionbase, region);
  region->regiontype = RGN_TYPE_UI;
  region->alignment = RGN_ALIGN_RIGHT;

  /* channels view */
  region = BKE_area_region_new();

  BLI_addtail(&sc->regionbase, region);
  region->regiontype = RGN_TYPE_CHANNELS;
  region->alignment = RGN_ALIGN_LEFT;

  region->v2d.scroll = V2D_SCROLL_BOTTOM;
  region->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

  /* preview view */
  region = BKE_area_region_new();

  BLI_addtail(&sc->regionbase, region);
  region->regiontype = RGN_TYPE_PREVIEW;

  /* main region */
  region = BKE_area_region_new();

  BLI_addtail(&sc->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  return (SpaceLink *)sc;
}

/* Doesn't free the space-link itself. */
static void clip_free(SpaceLink *sl)
{
  SpaceClip *sc = (SpaceClip *)sl;

  sc->clip = nullptr;

  if (sc->scopes.track_preview) {
    IMB_freeImBuf(sc->scopes.track_preview);
  }

  if (sc->scopes.track_search) {
    IMB_freeImBuf(sc->scopes.track_search);
  }
}

/* spacetype; init callback */
static void clip_init(wmWindowManager * /*wm*/, ScrArea *area)
{
  ListBase *lb = WM_dropboxmap_find("Clip", SPACE_CLIP, RGN_TYPE_WINDOW);

  /* add drop boxes */
  WM_event_add_dropbox_handler(&area->handlers, lb);
}

static SpaceLink *clip_duplicate(SpaceLink *sl)
{
  SpaceClip *scn = MEM_dupallocN("clip_duplicate", *reinterpret_cast<SpaceClip *>(sl));

  /* clear or remove stuff from old */
  scn->scopes.track_search = nullptr;
  scn->scopes.track_preview = nullptr;
  scn->scopes.ok = false;

  return (SpaceLink *)scn;
}

static void clip_listener(const wmSpaceTypeListenerParams *params)
{
  ScrArea *area = params->area;
  const wmNotifier *wmn = params->notifier;
  const Scene *scene = params->scene;

  /* context changes */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
          clip_scopes_tag_refresh(area);
          ATTR_FALLTHROUGH;

        case ND_FRAME_RANGE:
          ED_area_tag_redraw(area);
          break;
      }
      break;
    case NC_MOVIECLIP:
      switch (wmn->data) {
        case ND_DISPLAY:
        case ND_SELECT:
          clip_scopes_tag_refresh(area);
          ED_area_tag_redraw(area);
          break;
      }
      switch (wmn->action) {
        case NA_REMOVED:
        case NA_EDITED:
        case NA_EVALUATED:
          /* fall-through */

        case NA_SELECTED:
          clip_scopes_tag_refresh(area);
          ED_area_tag_redraw(area);
          break;
      }
      break;
    case NC_MASK:
      switch (wmn->data) {
        case ND_SELECT:
        case ND_DATA:
        case ND_DRAW:
          ED_area_tag_redraw(area);
          break;
      }
      switch (wmn->action) {
        case NA_SELECTED:
          ED_area_tag_redraw(area);
          break;
        case NA_EDITED:
          ED_area_tag_redraw(area);
          break;
      }
      break;
    case NC_GEOM:
      switch (wmn->data) {
        case ND_SELECT:
          clip_scopes_tag_refresh(area);
          ED_area_tag_redraw(area);
          break;
      }
      break;
    case NC_SCREEN:
      switch (wmn->data) {
        case ND_ANIMPLAY:
          ED_area_tag_redraw(area);
          break;
        case ND_LAYOUTSET:
          clip_area_sync_frame_from_scene(area, scene);
          break;
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_CLIP) {
        clip_scopes_tag_refresh(area);
        ED_area_tag_redraw(area);
      }
      break;
    case NC_GPENCIL:
      if (wmn->action == NA_EDITED) {
        clip_scopes_check_gpencil_change(area);
        ED_area_tag_redraw(area);
      }
      else if (wmn->data & ND_GPENCIL_EDITMODE) {
        ED_area_tag_redraw(area);
      }
      break;
    case NC_WM:
      switch (wmn->data) {
        case ND_FILEREAD:
        case ND_UNDO:
          clip_area_sync_frame_from_scene(area, scene);
          break;
      }
      break;
  }
}

static void clip_operatortypes()
{
  /* `clip_ops.cc` */
  WM_operatortype_append(CLIP_OT_open);
  WM_operatortype_append(CLIP_OT_reload);
  WM_operatortype_append(CLIP_OT_view_pan);
  WM_operatortype_append(CLIP_OT_view_zoom);
  WM_operatortype_append(CLIP_OT_view_zoom_in);
  WM_operatortype_append(CLIP_OT_view_zoom_out);
  WM_operatortype_append(CLIP_OT_view_zoom_ratio);
  WM_operatortype_append(CLIP_OT_view_all);
  WM_operatortype_append(CLIP_OT_view_selected);
  WM_operatortype_append(CLIP_OT_view_center_cursor);
  WM_operatortype_append(CLIP_OT_change_frame);
  WM_operatortype_append(CLIP_OT_rebuild_proxy);
  WM_operatortype_append(CLIP_OT_mode_set);
#ifdef WITH_INPUT_NDOF
  WM_operatortype_append(CLIP_OT_view_ndof);
#endif
  WM_operatortype_append(CLIP_OT_prefetch);
  WM_operatortype_append(CLIP_OT_set_scene_frames);
  WM_operatortype_append(CLIP_OT_cursor_set);
  WM_operatortype_append(CLIP_OT_lock_selection_toggle);

  /* `tracking_ops.cc` */

  /* navigation */
  WM_operatortype_append(CLIP_OT_frame_jump);

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
  WM_operatortype_append(CLIP_OT_average_tracks);
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

  WM_operatortype_append(CLIP_OT_new_image_from_plane_marker);
  WM_operatortype_append(CLIP_OT_update_image_from_plane_marker);

  /* `clip_graph_ops.cc` */

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

  /* `clip_dopesheet_ops.cc` */

  WM_operatortype_append(CLIP_OT_dopesheet_select_channel);
  WM_operatortype_append(CLIP_OT_dopesheet_view_all);
}

static void clip_keymap(wmKeyConfig *keyconf)
{
  /* ******** Global hotkeys available for all regions ******** */
  WM_keymap_ensure(keyconf, "Clip", SPACE_CLIP, RGN_TYPE_WINDOW);

  /* ******** Hotkeys available for main region only ******** */
  WM_keymap_ensure(keyconf, "Clip Editor", SPACE_CLIP, RGN_TYPE_WINDOW);
  //  keymap->poll = ED_space_clip_tracking_poll;

  /* ******** Hotkeys available for preview region only ******** */
  WM_keymap_ensure(keyconf, "Clip Graph Editor", SPACE_CLIP, RGN_TYPE_WINDOW);

  /* ******** Hotkeys available for channels region only ******** */
  WM_keymap_ensure(keyconf, "Clip Dopesheet Editor", SPACE_CLIP, RGN_TYPE_WINDOW);
}

/* DO NOT make this static, this hides the symbol and breaks API generation script. */
extern "C" const char *clip_context_dir[]; /* quiet warning. */
const char *clip_context_dir[] = {"edit_movieclip", "edit_mask", nullptr};

static int /*eContextResult*/ clip_context(const bContext *C,
                                           const char *member,
                                           bContextDataResult *result)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, clip_context_dir);

    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "edit_movieclip")) {
    if (sc->clip) {
      CTX_data_id_pointer_set(result, &sc->clip->id);
    }
    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "edit_mask")) {
    if (sc->mask_info.mask) {
      CTX_data_id_pointer_set(result, &sc->mask_info.mask->id);
    }
    return CTX_RESULT_OK;
  }

  return CTX_RESULT_MEMBER_NOT_FOUND;
}

/* dropboxes */
static bool clip_drop_poll(bContext * /*C*/, wmDrag *drag, const wmEvent * /*event*/)
{
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    if (ELEM(file_type, FILE_TYPE_IMAGE, FILE_TYPE_MOVIE)) {
      return true;
    }
  }

  return false;
}

static void clip_drop_copy(bContext * /*C*/, wmDrag *drag, wmDropBox *drop)
{
  PointerRNA itemptr;
  char dir[FILE_MAX], file[FILE_MAX];

  BLI_path_split_dir_file(WM_drag_get_single_path(drag), dir, sizeof(dir), file, sizeof(file));

  RNA_string_set(drop->ptr, "directory", dir);

  RNA_collection_clear(drop->ptr, "files");
  RNA_collection_add(drop->ptr, "files", &itemptr);
  RNA_string_set(&itemptr, "name", file);
}

/* area+region dropbox definition */
static void clip_dropboxes()
{
  ListBase *lb = WM_dropboxmap_find("Clip", SPACE_CLIP, RGN_TYPE_WINDOW);

  WM_dropbox_add(lb, "CLIP_OT_open", clip_drop_poll, clip_drop_copy, nullptr, nullptr);
}

static void clip_refresh(const bContext *C, ScrArea *area)
{
  Scene *scene = CTX_data_scene(C);
  SpaceClip *sc = (SpaceClip *)area->spacedata.first;

  ARegion *region_preview = BKE_area_find_region_type(area, RGN_TYPE_PREVIEW);
  if (!(region_preview->v2d.flag & V2D_IS_INIT)) {
    init_preview_region(scene, area, sc, region_preview);
    region_preview->v2d.cur = region_preview->v2d.tot;
  }
  /* #V2D_VIEWSYNC_AREA_VERTICAL must always be set for the dopesheet view, in graph view it must
   * be unset. This is enforced by region re-initialization.
   * That means if it's not set correctly, the view just changed and needs re-initialization */
  else if (sc->view == SC_VIEW_DOPESHEET) {
    if ((region_preview->v2d.flag & V2D_VIEWSYNC_AREA_VERTICAL) == 0) {
      init_preview_region(scene, area, sc, region_preview);
    }
  }
  else {
    if (region_preview->v2d.flag & V2D_VIEWSYNC_AREA_VERTICAL) {
      init_preview_region(scene, area, sc, region_preview);
    }
  }

  BKE_movieclip_user_set_frame(&sc->user, scene->r.cfra);
}

static void CLIP_GGT_navigate(wmGizmoGroupType *gzgt)
{
  VIEW2D_GGT_navigate_impl(gzgt, "CLIP_GGT_navigate");
}

static void clip_gizmos()
{
  const wmGizmoMapType_Params gizmo_params{SPACE_CLIP, RGN_TYPE_WINDOW};
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gizmo_params);

  WM_gizmogrouptype_append_and_link(gzmap_type, CLIP_GGT_navigate);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Region
 * \{ */

/* sets up the fields of the View2D from zoom and offset */
static void movieclip_main_area_set_view2d(const bContext *C, ARegion *region)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  float x1, y1, w, h, aspx, aspy;
  int width, height, winx, winy;

  ED_space_clip_get_size(sc, &width, &height);
  ED_space_clip_get_aspect(sc, &aspx, &aspy);

  w = width * aspx;
  h = height * aspy;

  winx = BLI_rcti_size_x(&region->winrct) + 1;
  winy = BLI_rcti_size_y(&region->winrct) + 1;

  region->v2d.tot.xmin = 0;
  region->v2d.tot.ymin = 0;
  region->v2d.tot.xmax = w;
  region->v2d.tot.ymax = h;

  region->v2d.mask.xmin = region->v2d.mask.ymin = 0;
  region->v2d.mask.xmax = winx;
  region->v2d.mask.ymax = winy;

  /* which part of the image space do we see? */
  x1 = region->winrct.xmin + (winx - sc->zoom * w) / 2.0f;
  y1 = region->winrct.ymin + (winy - sc->zoom * h) / 2.0f;

  x1 -= sc->zoom * sc->xof;
  y1 -= sc->zoom * sc->yof;

  /* relative display right */
  region->v2d.cur.xmin = (region->winrct.xmin - x1) / sc->zoom;
  region->v2d.cur.xmax = region->v2d.cur.xmin + (float(winx) / sc->zoom);

  /* relative display left */
  region->v2d.cur.ymin = (region->winrct.ymin - y1) / sc->zoom;
  region->v2d.cur.ymax = region->v2d.cur.ymin + (float(winy) / sc->zoom);

  /* normalize 0.0..1.0 */
  region->v2d.cur.xmin /= w;
  region->v2d.cur.xmax /= w;
  region->v2d.cur.ymin /= h;
  region->v2d.cur.ymax /= h;
}

static bool clip_main_region_poll(const RegionPollParams *params)
{
  const SpaceClip *sclip = static_cast<SpaceClip *>(params->area->spacedata.first);
  return ELEM(sclip->view, SC_VIEW_CLIP);
}

/* add handlers, stuff you only do once or on area/region changes */
static void clip_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  /* NOTE: don't use `UI_view2d_region_reinit(&region->v2d, ...)`
   * since the space clip manages its own v2d in #movieclip_main_area_set_view2d */

  /* mask polls mode */
  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Mask Editing", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Clip", SPACE_CLIP, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Clip Editor", SPACE_CLIP, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);
}

static void clip_main_region_draw(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  float aspx, aspy, zoomx, zoomy, x, y;
  int width, height;
  bool show_cursor = false;

  /* If tracking is in progress, we should synchronize the frame from the clip-user
   * (#MovieClipUser.framenr) so latest tracked frame would be shown. */
  if (clip && clip->tracking_context) {
    BKE_autotrack_context_sync_user(static_cast<AutoTrackContext *>(clip->tracking_context),
                                    &sc->user);
  }

  if (sc->flag & SC_LOCK_SELECTION) {
    ImBuf *tmpibuf = nullptr;

    if (clip && clip->tracking.stabilization.flag & TRACKING_2D_STABILIZATION) {
      tmpibuf = ED_space_clip_get_stable_buffer(sc, nullptr, nullptr, nullptr);
    }

    if (ED_clip_view_selection(C, region, false)) {
      sc->xof += sc->xlockof;
      sc->yof += sc->ylockof;
    }

    if (tmpibuf) {
      IMB_freeImBuf(tmpibuf);
    }
  }

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);

  /* data... */
  movieclip_main_area_set_view2d(C, region);

  /* callback */
  ED_region_draw_cb_draw(C, region, REGION_DRAW_PRE_VIEW);

  clip_draw_main(C, sc, region);

  /* TODO(sergey): would be nice to find a way to de-duplicate all this space conversions */
  UI_view2d_view_to_region_fl(&region->v2d, 0.0f, 0.0f, &x, &y);
  ED_space_clip_get_size(sc, &width, &height);
  ED_space_clip_get_zoom(sc, region, &zoomx, &zoomy);
  ED_space_clip_get_aspect(sc, &aspx, &aspy);

  if (sc->mode == SC_MODE_MASKEDIT) {
    Mask *mask = CTX_data_edit_mask(C);
    if (mask && clip) {
      ScrArea *area = CTX_wm_area(C);
      int mask_width, mask_height;
      ED_mask_get_size(area, &mask_width, &mask_height);
      ED_mask_draw_region(CTX_data_expect_evaluated_depsgraph(C),
                          mask,
                          region,
                          sc->overlay.flag & SC_SHOW_OVERLAYS,
                          sc->mask_info.draw_flag,
                          sc->mask_info.draw_type,
                          eMaskOverlayMode(sc->mask_info.overlay_mode),
                          sc->mask_info.blend_factor,
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

  if (sc->overlay.flag & SC_SHOW_OVERLAYS && sc->overlay.flag & SC_SHOW_CURSOR && show_cursor) {
    GPU_matrix_push();
    GPU_matrix_translate_2f(x, y);
    GPU_matrix_scale_2f(zoomx, zoomy);
    GPU_matrix_mul(sc->stabmat);
    GPU_matrix_scale_2f(width, height);
    ED_image_draw_cursor(region, sc->cursor);
    GPU_matrix_pop();
  }

  clip_draw_cache_and_notes(C, sc, region);

  if (sc->overlay.flag & SC_SHOW_OVERLAYS && sc->flag & SC_SHOW_ANNOTATION) {
    /* Grease Pencil */
    clip_draw_grease_pencil((bContext *)C, true);
  }

  /* callback */
  /* TODO(sergey): For being consistent with space image the projection needs to be configured
   * the way how the commented out code does it. This works correct for tracking data, but it
   * causes wrong aspect correction for mask editor (see #84990). */
  // GPU_matrix_push_projection();
  // wmOrtho2(region->v2d.cur.xmin, region->v2d.cur.xmax, region->v2d.cur.ymin,
  //          region->v2d.cur.ymax);
  ED_region_draw_cb_draw(C, region, REGION_DRAW_POST_VIEW);
  // GPU_matrix_pop_projection();

  /* reset view matrix */
  UI_view2d_view_restore(C);

  if (sc->overlay.flag & SC_SHOW_OVERLAYS && sc->flag & SC_SHOW_ANNOTATION) {
    /* draw Grease Pencil - screen space only */
    clip_draw_grease_pencil((bContext *)C, false);
  }
  if ((sc->gizmo_flag & SCLIP_GIZMO_HIDE) == 0) {
    WM_gizmomap_draw(region->runtime->gizmo_map, C, WM_GIZMOMAP_DRAWSTEP_2D);
  }
}

static void clip_main_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_GPENCIL:
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      else if (wmn->data & ND_GPENCIL_EDITMODE) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Preview Region
 * \{ */

static bool clip_preview_region_poll(const RegionPollParams *params)
{
  const SpaceClip *sclip = static_cast<SpaceClip *>(params->area->spacedata.first);
  return ELEM(sclip->view, SC_VIEW_GRAPH, SC_VIEW_DOPESHEET);
}

static void clip_preview_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_CUSTOM, region->winx, region->winy);

  /* own keymap */

  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Clip", SPACE_CLIP, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Clip Time Scrub", SPACE_CLIP, RGN_TYPE_PREVIEW);
  WM_event_add_keymap_handler_poll(
      &region->runtime->handlers, keymap, ED_time_scrub_event_in_region_poll);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Clip Graph Editor", SPACE_CLIP, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Clip Dopesheet Editor", SPACE_CLIP, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);
}

static void graph_region_draw(const bContext *C, ARegion *region)
{
  View2D *v2d = &region->v2d;
  SpaceClip *sc = CTX_wm_space_clip(C);
  Scene *scene = CTX_data_scene(C);
  short cfra_flag = 0;
  const bool minimized = (region->winy <= HEADERY * UI_SCALE_FAC * 1.1f);

  if (sc->flag & SC_LOCK_TIMECURSOR) {
    ED_clip_graph_center_current_frame(scene, region);
  }

  /* clear and setup matrix */
  UI_ThemeClearColor(minimized ? TH_TIME_SCRUB_BACKGROUND : TH_BACK);

  UI_view2d_view_ortho(v2d);

  /* data... */
  clip_draw_graph(sc, region, scene);

  /* current frame indicator line */
  if (sc->flag & SC_SHOW_SECONDS) {
    cfra_flag |= DRAWCFRA_UNIT_SECONDS;
  }
  ANIM_draw_cfra(C, v2d, cfra_flag);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* time-scrubbing */
  const int fps = round_db_to_int(scene->frames_per_second());
  ED_time_scrub_draw(region, scene, sc->flag & SC_SHOW_SECONDS, true, fps);

  /* current frame indicator */
  ED_time_scrub_draw_current_frame(region, scene, sc->flag & SC_SHOW_SECONDS, !minimized);

  /* scrollers */
  if (!minimized) {
    const rcti scroller_mask = ED_time_scrub_clamp_scroller_mask(v2d->mask);
    region->v2d.scroll |= V2D_SCROLL_BOTTOM;
    UI_view2d_scrollers_draw(v2d, &scroller_mask);
  }
  else {
    region->v2d.scroll &= ~V2D_SCROLL_BOTTOM;
  }

  /* scale indicators */
  {
    rcti rect;
    BLI_rcti_init(
        &rect, 0, 15 * UI_SCALE_FAC, 15 * UI_SCALE_FAC, region->winy - UI_TIME_SCRUB_MARGIN_Y);
    UI_view2d_draw_scale_y__values(region, v2d, &rect, TH_TEXT, 10);
  }
}

static void dopesheet_region_draw(const bContext *C, ARegion *region)
{
  Scene *scene = CTX_data_scene(C);
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  View2D *v2d = &region->v2d;
  short cfra_flag = 0;
  const bool minimized = (region->winy <= HEADERY * UI_SCALE_FAC * 1.1f);

  if (clip) {
    BKE_tracking_dopesheet_update(&clip->tracking);
  }

  /* clear and setup matrix */
  UI_ThemeClearColor(minimized ? TH_TIME_SCRUB_BACKGROUND : TH_BACK);

  UI_view2d_view_ortho(v2d);

  /* time grid */
  if (!minimized) {
    UI_view2d_draw_lines_x__discrete_frames_or_seconds(
        v2d, scene, sc->flag & SC_SHOW_SECONDS, true);
  }

  /* data... */
  clip_draw_dopesheet_main(sc, region, scene);

  /* current frame indicator line */
  if (sc->flag & SC_SHOW_SECONDS) {
    cfra_flag |= DRAWCFRA_UNIT_SECONDS;
  }
  ANIM_draw_cfra(C, v2d, cfra_flag);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* time-scrubbing */
  const int fps = round_db_to_int(scene->frames_per_second());
  ED_time_scrub_draw(region, scene, sc->flag & SC_SHOW_SECONDS, true, fps);

  /* current frame indicator */
  ED_time_scrub_draw_current_frame(region, scene, sc->flag & SC_SHOW_SECONDS, !minimized);

  /* scrollers */
  if (!minimized) {
    region->v2d.scroll |= V2D_SCROLL_BOTTOM;
    UI_view2d_scrollers_draw(v2d, nullptr);
  }
  else {
    region->v2d.scroll &= ~V2D_SCROLL_BOTTOM;
  }
}

static void clip_preview_region_draw(const bContext *C, ARegion *region)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  if (sc->view == SC_VIEW_GRAPH) {
    graph_region_draw(C, region);
  }
  else if (sc->view == SC_VIEW_DOPESHEET) {
    dopesheet_region_draw(C, region);
  }
}

static void clip_preview_region_listener(const wmRegionListenerParams * /*params*/) {}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Channels Region
 * \{ */

static bool clip_channels_region_poll(const RegionPollParams *params)
{
  const SpaceClip *sclip = static_cast<SpaceClip *>(params->area->spacedata.first);
  return ELEM(sclip->view, SC_VIEW_DOPESHEET);
}

static void clip_channels_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  /* ensure the 2d view sync works - main region has bottom scroller */
  region->v2d.scroll = V2D_SCROLL_BOTTOM;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_LIST, region->winx, region->winy);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Clip Dopesheet Editor", SPACE_CLIP, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);
}

static void clip_channels_region_draw(const bContext *C, ARegion *region)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  View2D *v2d = &region->v2d;

  if (clip) {
    BKE_tracking_dopesheet_update(&clip->tracking);
  }

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);

  UI_view2d_view_ortho(v2d);

  /* data... */
  clip_draw_dopesheet_channels(C, region);

  /* reset view matrix */
  UI_view2d_view_restore(C);
}

static void clip_channels_region_listener(const wmRegionListenerParams * /*params*/) {}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Header Region
 * \{ */

/* add handlers, stuff you only do once or on area/region changes */
static void clip_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void clip_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

static void clip_header_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        /* for proportional editmode only */
        case ND_TOOLSETTINGS:
          /* TODO: should do this when in mask mode only but no data available. */
          // if (sc->mode == SC_MODE_MASKEDIT)
          {
            ED_region_tag_redraw(region);
            break;
          }
      }
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tools Region
 * \{ */

static bool clip_tools_region_poll(const RegionPollParams *params)
{
  const SpaceClip *sclip = static_cast<SpaceClip *>(params->area->spacedata.first);
  return ELEM(sclip->view, SC_VIEW_CLIP);
}

/* add handlers, stuff you only do once or on area/region changes */
static void clip_tools_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Clip", SPACE_CLIP, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
}

static void clip_tools_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tool Properties Region
 * \{ */

static void clip_props_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_WM:
      if (wmn->data == ND_HISTORY) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCENE:
      if (wmn->data == ND_MODE) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_CLIP) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_GPENCIL:
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Properties Region
 * \{ */

static bool clip_properties_region_poll(const RegionPollParams *params)
{
  const SpaceClip *sclip = static_cast<SpaceClip *>(params->area->spacedata.first);
  return ELEM(sclip->view, SC_VIEW_CLIP);
}

/* add handlers, stuff you only do once or on area/region changes */
static void clip_properties_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Clip", SPACE_CLIP, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
}

static void clip_properties_region_draw(const bContext *C, ARegion *region)
{
  SpaceClip *sc = CTX_wm_space_clip(C);

  BKE_movieclip_update_scopes(sc->clip, &sc->user, &sc->scopes);

  ED_region_panels(C, region);
}

static void clip_properties_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_GPENCIL:
      if (ELEM(wmn->data, ND_DATA, ND_GPENCIL_EDITMODE)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_BRUSH:
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name IO Callbacks
 * \{ */

static void clip_id_remap(ScrArea * /*area*/,
                          SpaceLink *slink,
                          const blender::bke::id::IDRemapper &mappings)
{
  SpaceClip *sclip = (SpaceClip *)slink;

  if (!mappings.contains_mappings_for_any(FILTER_ID_MC | FILTER_ID_MSK)) {
    return;
  }

  mappings.apply(reinterpret_cast<ID **>(&sclip->clip), ID_REMAP_APPLY_ENSURE_REAL);
  mappings.apply(reinterpret_cast<ID **>(&sclip->mask_info.mask), ID_REMAP_APPLY_ENSURE_REAL);
}

static void clip_foreach_id(SpaceLink *space_link, LibraryForeachIDData *data)
{
  SpaceClip *sclip = reinterpret_cast<SpaceClip *>(space_link);
  const int data_flags = BKE_lib_query_foreachid_process_flags_get(data);
  const bool is_readonly = (data_flags & IDWALK_READONLY) != 0;

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(
      data, sclip->clip, IDWALK_CB_USER_ONE | IDWALK_CB_DIRECT_WEAK_LINK);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(
      data, sclip->mask_info.mask, IDWALK_CB_USER_ONE | IDWALK_CB_DIRECT_WEAK_LINK);

  if (!is_readonly) {
    sclip->scopes.ok = 0;
  }
}

static void clip_space_blend_read_data(BlendDataReader * /*reader*/, SpaceLink *sl)
{
  SpaceClip *sclip = (SpaceClip *)sl;

  sclip->scopes.track_search = nullptr;
  sclip->scopes.track_preview = nullptr;
  sclip->scopes.ok = 0;
}

static void clip_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  BLO_write_struct(writer, SpaceClip, sl);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_spacetype_clip()
{
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_CLIP;
  STRNCPY_UTF8(st->name, "Clip");

  st->create = clip_create;
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
  st->foreach_id = clip_foreach_id;
  st->blend_read_data = clip_space_blend_read_data;
  st->blend_read_after_liblink = nullptr;
  st->blend_write = clip_space_blend_write;

  /* regions: main window */
  art = MEM_callocN<ARegionType>("spacetype clip region");
  art->regionid = RGN_TYPE_WINDOW;
  art->poll = clip_main_region_poll;
  art->init = clip_main_region_init;
  art->draw = clip_main_region_draw;
  art->listener = clip_main_region_listener;
  art->keymapflag = ED_KEYMAP_GIZMO | ED_KEYMAP_FRAMES | ED_KEYMAP_UI | ED_KEYMAP_GPENCIL;

  BLI_addhead(&st->regiontypes, art);

  /* preview */
  art = MEM_callocN<ARegionType>("spacetype clip region preview");
  art->regionid = RGN_TYPE_PREVIEW;
  art->prefsizey = 240;
  art->poll = clip_preview_region_poll;
  art->init = clip_preview_region_init;
  art->draw = clip_preview_region_draw;
  art->listener = clip_preview_region_listener;
  art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;

  BLI_addhead(&st->regiontypes, art);

  /* regions: properties */
  art = MEM_callocN<ARegionType>("spacetype clip region properties");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI;
  art->poll = clip_properties_region_poll;
  art->init = clip_properties_region_init;
  art->snap_size = ED_region_generic_panel_region_snap_size;
  art->draw = clip_properties_region_draw;
  art->listener = clip_properties_region_listener;
  BLI_addhead(&st->regiontypes, art);
  ED_clip_buttons_register(art);

  /* regions: tools */
  art = MEM_callocN<ARegionType>("spacetype clip region tools");
  art->regionid = RGN_TYPE_TOOLS;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI;
  art->poll = clip_tools_region_poll;
  art->listener = clip_props_region_listener;
  art->init = clip_tools_region_init;
  art->draw = clip_tools_region_draw;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN<ARegionType>("spacetype clip region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

  art->init = clip_header_region_init;
  art->draw = clip_header_region_draw;
  art->listener = clip_header_region_listener;

  BLI_addhead(&st->regiontypes, art);

  /* channels */
  art = MEM_callocN<ARegionType>("spacetype clip channels region");
  art->regionid = RGN_TYPE_CHANNELS;
  art->prefsizex = UI_COMPACT_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI;
  art->poll = clip_channels_region_poll;
  art->listener = clip_channels_region_listener;
  art->init = clip_channels_region_init;
  art->draw = clip_channels_region_draw;

  BLI_addhead(&st->regiontypes, art);

  /* regions: hud */
  art = ED_area_type_hud(st->spaceid);
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(std::move(st));
}

/** \} */
