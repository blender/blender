/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaction
 */

#include <cstdio>
#include <cstring>

#include "DNA_action_types.h"
#include "DNA_collection_types.h"
#include "DNA_scene_types.h"

#include "DNA_screen_types.h"
#include "MEM_guardedalloc.h"

#include "BKE_lib_id.hh"
#include "BLI_listbase.hh"
#include "BLI_math_base_c.hh"
#include "BLI_resource_scope.hh"
#include "BLI_string_utf8.hh"
#include "BLI_utildefines.hh"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main_idmap.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "ED_anim_api.hh"
#include "ED_anim_filter.hh"
#include "ED_markers.hh"
#include "ED_screen.hh"
#include "ED_space_action.hh"
#include "ED_space_api.hh"
#include "ED_time_scrub_ui.hh"

#include "BLO_read_write.hh"

#include "GPU_matrix.hh"

#include "action_intern.hh" /* own include */

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Default Callbacks for Action Space
 * \{ */

/* Redraw only. */
static void redraw(ARegion *region)
{
  ED_region_tag_redraw(region);
}

static void rebuild_and_redraw(ScrArea * /*area*/, ARegion *region)
{
  redraw(region);
}

static SpaceLink *action_create(const ScrArea *area, const Scene *scene)
{
  SpaceAction *saction;
  ARegion *region;

  saction = MEM_new<SpaceAction>("initaction");
  saction->spacetype = SPACE_ACTION;
  saction->runtime = MEM_new<SpaceAction_Runtime>(__func__);

  const eAnimEdit_Context desired_mode = area ? eAnimEdit_Context(area->butspacetype_subtype) :
                                                SACTCONT_DOPESHEET;
  const bool is_timeline = (desired_mode == SACTCONT_TIMELINE);

  /* This should always set to SACTCONT_DOPESHEET, regardless of what the desired_mode is set to.
   * Not for fundamental reasons, but to make it safe to call this function with an invalid value
   * in desired_mode. I (Sybren) have no idea if that's ever going to happen, but in this case I'm
   * sticking as close as possible to what Blender 4.5 was already doing. Once this function
   * returns, ED_area_newspace() will call action_space_subtype_set() to set the sub-type. */
  saction->mode = SACTCONT_DOPESHEET;
  saction->mode_prev = SACTCONT_DOPESHEET;
  saction->flag = SACTION_SHOW_INTERPOLATION | SACTION_SHOW_MARKERS;

  saction->ads.filterflag |= ADS_FILTER_SUMMARY;
  if (is_timeline) {
    saction->ads.flag |= ADS_FLAG_SUMMARY_COLLAPSED;
  }

  saction->cache_display = TIME_CACHE_DISPLAY | TIME_CACHE_SOFTBODY | TIME_CACHE_PARTICLES |
                           TIME_CACHE_CLOTH | TIME_CACHE_SMOKE | TIME_CACHE_DYNAMICPAINT |
                           TIME_CACHE_RIGIDBODY | TIME_CACHE_SIMULATION_NODES |
                           TIME_CACHE_COMPOSITOR;

  saction->overlays.flag |= (ADS_OVERLAY_SHOW_OVERLAYS | ADS_SHOW_SCENE_STRIP_FRAME_RANGE);

  /* header */
  region = BKE_area_region_new();

  BLI_addtail(&saction->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* footer */
  region = BKE_area_region_new();

  BLI_addtail(&saction->regionbase, region);
  region->regiontype = RGN_TYPE_FOOTER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM;

  /* channel list region */
  region = BKE_area_region_new();
  BLI_addtail(&saction->regionbase, region);
  region->regiontype = RGN_TYPE_CHANNELS;
  region->alignment = RGN_ALIGN_LEFT;
  /* Channel list is hidden by default in timeline mode, and visible in other modes. */
  if (is_timeline) {
    region->flag |= RGN_FLAG_HIDDEN;
  }

  /* Only need to set scroll settings, as this will use `listview` v2d configuration. */
  region->v2d.scroll = V2D_SCROLL_BOTTOM;
  region->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

  /* ui buttons */
  region = BKE_area_region_new();

  BLI_addtail(&saction->regionbase, region);
  region->regiontype = RGN_TYPE_UI;
  region->alignment = RGN_ALIGN_RIGHT;

  /* main region */
  region = BKE_area_region_new();

  BLI_addtail(&saction->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  region->v2d.tot.xmin = float(scene->r.sfra - 10);
  region->v2d.tot.ymin = float(-area->winy) / 3.0f;
  region->v2d.tot.xmax = float(scene->r.efra + 10);
  region->v2d.tot.ymax = 0.0f;

  region->v2d.cur = region->v2d.tot;

  region->v2d.min[0] = 0.0f;
  region->v2d.min[1] = 0.0f;

  region->v2d.max[0] = MAXFRAMEF;
  region->v2d.max[1] = FLT_MAX;

  region->v2d.minzoom = 0.01f;
  region->v2d.maxzoom = 50;
  region->v2d.scroll = (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
  region->v2d.scroll |= V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
  region->v2d.keepzoom = V2D_LOCKZOOM_Y;
  region->v2d.keepofs = V2D_KEEPOFS_Y;
  region->v2d.align = V2D_ALIGN_NO_POS_Y;
  region->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

  return reinterpret_cast<SpaceLink *>(saction);
}

/* Doesn't free the space-link itself. */
static void action_free(SpaceLink *sl)
{
  SpaceAction *saction = reinterpret_cast<SpaceAction *>(sl);

  MEM_delete(saction->runtime);
}

/* spacetype; init callback */
static void action_init(wmWindowManager * /*wm*/, ScrArea *area)
{
  SpaceAction *saction = area->spacedata.first_as<SpaceAction>();
  saction->runtime->flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
}

static SpaceLink *action_duplicate(SpaceLink *sl)
{
  SpaceAction *saction = MEM_dupalloc(reinterpret_cast<SpaceAction *>(sl));

  saction->runtime = MEM_new<SpaceAction_Runtime>(__func__);

  /* TODO: copy the UI state. */

  return reinterpret_cast<SpaceLink *>(saction);
}

/** Get the channels, caching them on the space data. */
static ed::animfilter::ChannelArena &space_action_channel_arena(const bContext &C)
{
  SpaceAction *saction = CTX_wm_space_action(&C);
  SpaceAction_Runtime *runtime = saction->runtime;
  if (!runtime->channel_arena) {
    ed::animfilter::ChannelArena channels = ed::action::get_summary_tree(C);
    runtime->channel_arena.emplace(std::move(channels));
  }

  return *runtime->channel_arena;
}

/** Clean up any cached data that was built during the previous drawing of regions. */
static void action_draw_pre(const bContext * /*C*/, ScrArea *area)
{
  SpaceAction *saction = area->spacedata.first_as<SpaceAction>();
  saction->runtime->channel_arena.reset();
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  view2d_region_reinit(&region->v2d, ui::V2D_COMMONVIEW_CUSTOM, region->winx, region->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Dopesheet", SPACE_ACTION, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_poll(
      &region->runtime->handlers, keymap, WM_event_handler_region_v2d_mask_no_marker_poll);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Dopesheet Generic", SPACE_ACTION, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
}

static void set_v2d_height(View2D *v2d, const size_t item_count, const bool add_marker_padding)
{
  const int height = ANIM_UI_get_channels_total_height(v2d, item_count);
  float pad_bottom = add_marker_padding ? UI_MARKER_MARGIN_Y : 0;
  /* Add padding for the collapsed redo panel. */
  pad_bottom += HEADERY;
  v2d->tot.ymin = -(height + pad_bottom);
  ui::view2d_curRect_clamp_y(v2d);
}

static void action_main_region_draw(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  SpaceAction *saction = CTX_wm_space_action(C);
  Scene *scene = CTX_data_scene(C);
  View2D *v2d = &region->v2d;

  const int min_height = UI_ANIM_MINY;

  /* scrollers */
  if (region->winy >= UI_ANIM_MINY) {
    region->v2d.scroll |= V2D_SCROLL_BOTTOM;
  }
  else {
    region->v2d.scroll &= ~V2D_SCROLL_BOTTOM;
  }

  /* Clear with solid color. */
  ui::theme::frame_buffer_clear(TH_BACK);

  if (USER_EXPERIMENTAL_TEST(&U, use_new_animfilter)) {
    ed::animfilter::ChannelArena &channels = space_action_channel_arena(*C);
    ListBaseT<TimeMarker> *markers = ANIM_marker_list_from_context(*C);
    set_v2d_height(v2d, channels.num_channels(), !markers->is_empty());
    ui::view2d_view_ortho(v2d);

    if (region->winy > min_height) {
      /* Time grid. */
      ui::view2d_draw_lines_x_frames(v2d, scene, saction->flag & SACTION_DRAWTIME, false, true);
    }
    ED_region_draw_cb_draw(C, region, REGION_DRAW_PRE_VIEW);

    if (region->winy > min_height) {
      /* Dark region marking scene start and end frame. */
      ANIM_draw_framerange(scene, v2d);
    }

    ed::animfilter::draw_dopesheet_key_area(*channels.root(), *v2d);

    /* TODO draw markers. */
    ANIM_draw_previewrange(scene, v2d, 0);
    ANIM_draw_scene_strip_range(C, v2d);

    ED_region_draw_cb_draw(C, region, REGION_DRAW_POST_VIEW);

    ui::view2d_view_restore(C);

    WM_gizmomap_draw(region->runtime->gizmo_map, C, WM_GIZMOMAP_DRAWSTEP_2D);

    const int fps = round_db_to_int(scene->frames_per_second());
    ED_time_scrub_draw(region, scene, saction->flag & SACTION_DRAWTIME, true, fps);
  }
  else {
    ListBaseT<bAnimListElem> anim_data = {nullptr, nullptr};
    bAnimContext ac;
    const bool has_anim_context = ANIM_animdata_get_context(C, &ac);
    if (has_anim_context) {
      /* Build list of channels to draw. */
      const eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                                        ANIMFILTER_LIST_CHANNELS);
      const size_t items = ANIM_animdata_filter(
          &ac, &anim_data, filter, ac.data, eAnimCont_Types(ac.datatype));
      /* The View2D's height needs to be set before calling view2d_view_ortho because the latter
       * uses the View2D's `cur` rect which might be modified when setting the height. */
      set_v2d_height(v2d, items, !ac.markers->is_empty());
    }
    ui::view2d_view_ortho(v2d);

    /* time grid */
    if (region->winy > min_height) {
      ui::view2d_draw_lines_x_frames(v2d, scene, saction->flag & SACTION_DRAWTIME, false, true);
    }

    ED_region_draw_cb_draw(C, region, REGION_DRAW_PRE_VIEW);

    /* start and end frame */
    if (region->winy > min_height) {
      ANIM_draw_framerange(scene, v2d);
    }

    /* Draw the manually set intended playback frame range highlight in the Action editor. */
    if (ac.active_action) {
      AnimData *adt = BKE_animdata_from_id(ac.active_action_user);
      ANIM_draw_action_framerange(adt, ac.active_action, v2d, -FLT_MAX, FLT_MAX);
    }

    /* data */
    if (has_anim_context) {
      draw_channel_strips(&ac, saction, region, &anim_data);
    }

    /* markers */
    ui::view2d_view_orthoSpecial(region, v2d, true);

    const short marker_flag = ((ac.markers && (ac.markers != &ac.scene->markers)) ?
                                   DRAW_MARKERS_LOCAL :
                                   0) |
                              DRAW_MARKERS_MARGIN;

    if (ED_markers_region_visible(CTX_wm_area(C), region)) {
      ED_markers_draw(C, marker_flag);
    }

    /* preview range */
    ui::view2d_view_ortho(v2d);
    ANIM_draw_previewrange(scene, v2d, 0);

    ANIM_draw_scene_strip_range(C, v2d);

    /* callback */
    ui::view2d_view_ortho(v2d);
    ED_region_draw_cb_draw(C, region, REGION_DRAW_POST_VIEW);

    /* reset view matrix */
    ui::view2d_view_restore(C);

    /* gizmos */
    WM_gizmomap_draw(region->runtime->gizmo_map, C, WM_GIZMOMAP_DRAWSTEP_2D);

    /* scrubbing region */
    const int fps = round_db_to_int(scene->frames_per_second());
    ED_time_scrub_draw(region, scene, saction->flag & SACTION_DRAWTIME, true, fps);
  }
}

static void action_main_region_draw_overlay(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  const SpaceAction *saction = CTX_wm_space_action(C);
  const Scene *scene = CTX_data_scene(C);
  const Object *obact = CTX_data_active_object(C);
  View2D *v2d = &region->v2d;

  /* caches */
  GPU_matrix_push_projection();
  ui::view2d_view_orthoSpecial(region, v2d, true);
  timeline_draw_cache(saction, obact, scene);
  GPU_matrix_pop_projection();

  /* scrubbing region */
  ED_time_scrub_draw_current_frame(
      region, scene, saction->flag & SACTION_DRAWTIME, region->winy >= UI_ANIM_MINY);

  /* scrollers */
  const rcti scroller_mask = ED_time_scrub_clamp_scroller_mask(v2d->mask);
  ui::view2d_scrollers_draw(v2d, &scroller_mask);
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_channel_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  /* ensure the 2d view sync works - main region has bottom scroller */
  region->v2d.scroll = V2D_SCROLL_BOTTOM;

  view2d_region_reinit(&region->v2d, ui::V2D_COMMONVIEW_LIST, region->winx, region->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Animation Channels", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Dopesheet Generic", SPACE_ACTION, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
}

static void action_channel_region_draw(const bContext *C, ARegion *region)
{
  /* clear and setup matrix */
  ui::theme::frame_buffer_clear(TH_BACK);
  View2D *v2d = &region->v2d;

  if (USER_EXPERIMENTAL_TEST(&U, use_new_animfilter)) {
    ed::animfilter::ChannelArena &channels = space_action_channel_arena(*C);
    /* The View2D's height needs to be set before calling view2d_view_ortho because the latter
     * uses the View2D's `cur` rect which might be modified when setting the height. */
    /* TODO respect markers region. */
    ListBaseT<TimeMarker> *markers = ANIM_marker_list_from_context(*C);
    const bool has_markers = markers != nullptr && !markers->is_empty();
    set_v2d_height(v2d, channels.num_channels(), has_markers);
    ui::view2d_view_ortho(v2d);
    ed::animfilter::draw_channel_tree(*C, *channels.root(), *v2d);
    ui::view2d_view_restore(C);
  }
  else {
    /* draw entirely, view changes should be handled here */
    bAnimContext ac;
    const bool has_valid_animcontext = ANIM_animdata_get_context(C, &ac);

    if (!has_valid_animcontext) {
      return;
    }

    ListBaseT<bAnimListElem> anim_data = {nullptr, nullptr};
    /* Build list of channels to draw. */
    const eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                                      ANIMFILTER_LIST_CHANNELS);
    const size_t item_count = ANIM_animdata_filter(
        &ac, &anim_data, filter, ac.data, eAnimCont_Types(ac.datatype));
    /* The View2D's height needs to be set before calling view2d_view_ortho because the latter
     * uses the View2D's `cur` rect which might be modified when setting the height. */
    set_v2d_height(v2d, item_count, !ac.markers->is_empty());

    ui::view2d_view_ortho(v2d);
    draw_channel_names(const_cast<bContext *>(C), &ac, region, anim_data);

    /* channel filter next to scrubbing area */
    ED_time_scrub_channel_search_draw(C, region, ac.ads);

    /* reset view matrix */
    ui::view2d_view_restore(C);

    /* no scrollers here */
    ANIM_animdata_freelist(&anim_data);
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void action_header_region_draw(const bContext *C, ARegion *region)
{
  /* The anim context is not actually used, but this makes sure the action being displayed is up to
   * date. */
  bAnimContext ac;
  ANIM_animdata_get_context(C, &ac);

  ED_region_header(C, region);
}

static void action_channel_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_ANIMATION:
      rebuild_and_redraw(params->area, region);
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_OB_ACTIVE:
        case ND_FRAME:
          rebuild_and_redraw(params->area, region);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_KEYS:
          rebuild_and_redraw(params->area, region);
          break;
        case ND_MODIFIER:
          if (wmn->action == NA_RENAME) {
            rebuild_and_redraw(params->area, region);
          }
          break;
      }
      break;
    case NC_GPENCIL:
      if (ELEM(wmn->action, NA_RENAME, NA_SELECTED, NA_EDITED)) {
        rebuild_and_redraw(params->area, region);
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        rebuild_and_redraw(params->area, region);
      }
      break;
    default:
      if (wmn->data == ND_KEYS) {
        rebuild_and_redraw(params->area, region);
      }
      break;
  }
}

static void saction_channel_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
{
  wmMsgBus *mbus = params->message_bus;
  ARegion *region = params->region;

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw{};
  msg_sub_value_region_tag_redraw.owner = region;
  msg_sub_value_region_tag_redraw.user_data = region;
  msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;

  /* All dope-sheet filter settings, etc. affect the drawing of this editor,
   * also same applies for all animation-related data-types that may appear here,
   * so just whitelist the entire structs for updates. */
  {
    wmMsgParams_RNA msg_key_params = {{}};
    StructRNA *type_array[] = {
        RNA_DopeSheet, /* Dope-sheet filters. */

        RNA_ActionGroup, /* channel groups */

        RNA_FCurve, /* F-Curve */
        RNA_Keyframe,
        RNA_FCurveSample,

        RNA_Annotation, /* Grease Pencil */
        RNA_AnnotationLayer,
        RNA_AnnotationFrame,
    };

    for (int i = 0; i < ARRAY_SIZE(type_array); i++) {
      msg_key_params.ptr.type = type_array[i];
      WM_msg_subscribe_rna_params(
          mbus, &msg_key_params, &msg_sub_value_region_tag_redraw, __func__);
    }
  }
}

static void action_main_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_ANIMATION:
      rebuild_and_redraw(params->area, region);
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_RENDER_OPTIONS:
        case ND_OB_ACTIVE:
        case ND_FRAME:
        case ND_FRAME_RANGE:
        case ND_MARKERS:
          rebuild_and_redraw(params->area, region);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_TRANSFORM:
          /* moving object shouldn't need to redraw action */
          break;
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_BONE_COLLECTION:
        case ND_KEYS:
          rebuild_and_redraw(params->area, region);
          break;
      }
      break;
    case NC_NODE:
      switch (wmn->action) {
        case NA_EDITED:
          rebuild_and_redraw(params->area, region);
          break;
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        rebuild_and_redraw(params->area, region);
      }
      break;
    case NC_SCREEN:
      if (ELEM(wmn->data, ND_LAYER)) {
        rebuild_and_redraw(params->area, region);
      }
      break;
    default:
      if (wmn->data == ND_KEYS) {
        rebuild_and_redraw(params->area, region);
      }
      break;
  }
}

static void saction_main_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
{
  wmMsgBus *mbus = params->message_bus;
  Scene *scene = params->scene;
  ARegion *region = params->region;

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw{};
  msg_sub_value_region_tag_redraw.owner = region;
  msg_sub_value_region_tag_redraw.user_data = region;
  msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;

  /* Timeline depends on scene properties. */
  {
    bool use_preview = (scene->r.flag & SCER_PRV_RANGE);
    const PropertyRNA *props[] = {
        use_preview ? &rna_Scene_frame_preview_start : &rna_Scene_frame_start,
        use_preview ? &rna_Scene_frame_preview_end : &rna_Scene_frame_end,
        &rna_Scene_use_preview_range,
        &rna_Scene_frame_current,
    };

    PointerRNA idptr = RNA_id_pointer_create(&scene->id);

    for (int i = 0; i < ARRAY_SIZE(props); i++) {
      WM_msg_subscribe_rna(mbus, &idptr, props[i], &msg_sub_value_region_tag_redraw, __func__);
    }
  }

  /* Now run the general "channels region" one - since channels and main should be in sync */
  saction_channel_region_message_subscribe(params);
}

/* editor level listener */
static void action_listener(const wmSpaceTypeListenerParams *params)
{
  ScrArea *area = params->area;
  const wmNotifier *wmn = params->notifier;
  SpaceAction *saction = area->spacedata.first_as<SpaceAction>();

  /* context changes */
  switch (wmn->category) {
    case NC_GPENCIL:
      /* only handle these events for containers in which GPencil frames are displayed */
      if (ELEM(saction->mode, SACTCONT_GPENCIL, SACTCONT_DOPESHEET)) {
        if (wmn->action == NA_EDITED) {
          ED_area_tag_redraw(area);
        }
        else if (wmn->action == NA_SELECTED) {
          saction->runtime->flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(area);
        }
      }
      break;
    case NC_ANIMATION:
      /* For NLA tweak-mode enter/exit, need complete refresh. */
      if (wmn->data == ND_NLA_ACTCHANGE) {
        saction->runtime->flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
        ED_area_tag_refresh(area);
      }
      /* Auto-color only really needs to change when channels are added/removed,
       * or previously hidden stuff appears
       * (assume for now that if just adding these works, that will be fine).
       */
      else if (((wmn->data == ND_KEYFRAME) && ELEM(wmn->action, NA_ADDED, NA_REMOVED)) ||
               ((wmn->data == ND_ANIMCHAN) && (wmn->action != NA_SELECTED)))
      {
        ED_area_tag_refresh(area);
      }
      /* for simple edits to the curve data though (or just plain selections),
       * a simple redraw should work
       * (see #39851 for an example of how this can go wrong)
       */
      else {
        ED_area_tag_redraw(area);
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_SEQUENCER:
          if (wmn->action == NA_SELECTED) {
            saction->runtime->flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
            ED_area_tag_refresh(area);
          }
          break;
        case ND_OB_ACTIVE:
        case ND_OB_SELECT:
          /* Selection changed, so force refresh to flush
           * (needs flag set to do syncing). */
          saction->runtime->flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(area);
          break;
        case ND_RENDER_RESULT:
          ED_area_tag_redraw(area);
          break;
        case ND_FRAME_RANGE:
          for (ARegion &region : area->regionbase) {
            if (region.regiontype == RGN_TYPE_WINDOW) {
              Scene *scene = static_cast<Scene *>(wmn->reference);
              region.v2d.tot.xmin = float(scene->r.sfra - 4);
              region.v2d.tot.xmax = float(scene->r.efra + 4);
              break;
            }
          }
          break;
        default:
          /* Just redrawing the view will do. */
          ED_area_tag_redraw(area);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_SELECT: /* Selection changed, so force refresh to flush
                              * (needs flag set to do syncing). */
        case ND_BONE_ACTIVE:
          saction->runtime->flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(area);
          break;
        case ND_TRANSFORM:
          /* moving object shouldn't need to redraw action */
          break;
        case ND_POINTCACHE:
        case ND_MODIFIER:
        case ND_PARTICLE:
          break;
        default: /* just redrawing the view will do */
          ED_area_tag_redraw(area);
          break;
      }
      break;
    case NC_MASK:
      if (saction->mode == SACTCONT_MASK) {
        switch (wmn->data) {
          case ND_DATA:
            ED_area_tag_refresh(area);
            ED_area_tag_redraw(area);
            break;
          default: /* just redrawing the view will do */
            ED_area_tag_redraw(area);
            break;
        }
      }
      break;
    case NC_NODE:
      if (wmn->action == NA_SELECTED) {
        /* selection changed, so force refresh to flush (needs flag set to do syncing) */
        saction->runtime->flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
        ED_area_tag_refresh(area);
      }
      break;
    case NC_SPACE:
      switch (wmn->data) {
        case ND_SPACE_DOPESHEET:
          ED_area_tag_redraw(area);
          break;
        case ND_SPACE_TIME:
          ED_area_tag_redraw(area);
          break;
        case ND_SPACE_CHANGED:
          saction->runtime->flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(area);
          break;
      }
      break;
    case NC_WINDOW:
      if (saction->runtime->flag & SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC) {
        /* force redraw/refresh after undo/redo, see: #28962. */
        ED_area_tag_refresh(area);
      }
      break;
    case NC_WM:
      switch (wmn->data) {
        case ND_FILEREAD:
          ED_area_tag_refresh(area);
          break;
      }
      break;
  }
}

static void action_header_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_SCREEN:
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_OB_ACTIVE:
          rebuild_and_redraw(params->area, region);
          break;
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        rebuild_and_redraw(params->area, region);
      }
      break;
    case NC_ANIMATION:
      switch (wmn->data) {
        case ND_ANIMCHAN: /* set of visible animchannels changed */
          /* NOTE: for now, this should usually just mean that the filters changed
           *       It may be better if we had a dedicated flag for that though
           */
          rebuild_and_redraw(params->area, region);
          break;

        case ND_KEYFRAME: /* new keyframed added -> active action may have changed */
          // saction->flag |= SACTION_TEMP_NEEDCHANSYNC;
          rebuild_and_redraw(params->area, region);
          break;
      }
      break;
  }
}

static void action_footer_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_SCREEN:
      if (wmn->data == ND_ANIMPLAY) {
        rebuild_and_redraw(params->area, region);
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
          rebuild_and_redraw(params->area, region);
          break;
      }
      break;
  }
}

static bool action_region_poll_hide_in_timeline(const RegionPollParams *params)
{
  BLI_assert(params->area->spacetype == SPACE_ACTION);
  const SpaceAction *saction = params->area->spacedata.first_as<SpaceAction>();
  return saction->mode != SACTCONT_TIMELINE;
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_buttons_area_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Dopesheet Generic", SPACE_ACTION, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
}

static void action_buttons_area_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static void action_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_ANIMATION:
      rebuild_and_redraw(params->area, region);
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_OB_ACTIVE:
        case ND_FRAME:
        case ND_MARKERS:
          rebuild_and_redraw(params->area, region);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_KEYS:
          rebuild_and_redraw(params->area, region);
          break;
      }
      break;
    default:
      if (wmn->data == ND_KEYS) {
        rebuild_and_redraw(params->area, region);
      }
      break;
  }
}

static void action_refresh(const bContext *C, ScrArea *area)
{
  SpaceAction *saction = area->spacedata.first_as<SpaceAction>();

  /* Update the state of the animchannels in response to changes from the data they represent
   * NOTE: the temp flag is used to indicate when this needs to be done,
   * and will be cleared once handled. */
  if (saction->runtime->flag & SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC) {
    /* Perform syncing of channel state incl. selection
     * Active action setting also occurs here
     * (as part of anim channel filtering in `anim_filter.cc`). */
    ANIM_sync_animchannels_to_data(C);
    saction->runtime->flag &= ~SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;

    /* Tag everything for redraw
     * - Regions (such as header) need to be manually tagged for redraw too
     *   or else they don't update #28962.
     */
    ED_area_tag_redraw(area);
    for (ARegion &region : area->regionbase) {
      ED_region_tag_redraw(&region);
    }
  }

  /* region updates? */
  /* XXX re-sizing y-extents of tot should go here? */
}

static void action_id_remap(ScrArea * /*area*/,
                            SpaceLink *slink,
                            const bke::id::IDRemapper &mappings)
{
  using namespace ed::animfilter;

  SpaceAction *sact = reinterpret_cast<SpaceAction *>(slink);

  mappings.apply(reinterpret_cast<ID **>(&sact->ads.filter_grp), ID_REMAP_APPLY_DEFAULT);
  mappings.apply(&sact->ads.source, ID_REMAP_APPLY_DEFAULT);

  if (sact->runtime->channel_states.is_empty()) {
    return;
  }

  // /* Remap the ID pointers in the UI state map. */
  // printf("\033[95maction_id_remap: remapping %ld channel states\033[0m\n",
  //        sact->runtime->channel_states.size());
  // ChannelUIStateMap remapped_ui_state;
  // for (const auto &ui_state_item : sact->runtime->channel_states.items()) {
  //   /* Create a copy of the key, because map keys shouldn't be remapped. Instead, they should be
  //    * removed & re-inserted. */
  //   ChannelIdentifier new_ident = ui_state_item.key;
  //   switch (mappings.apply(&new_ident.owner_id, ID_REMAP_APPLY_DEFAULT)) {
  //     case IDRemapperApplyResult::ID_REMAP_RESULT_SOURCE_UNAVAILABLE:
  //       /* No remapping for this owner_id, so keep the item as-is. */
  //       printf("    kept %p\n", ui_state_item.key.owner_id);
  //       remapped_ui_state.add_new(ui_state_item.key, ui_state_item.value);
  //       break;
  //     case IDRemapperApplyResult::ID_REMAP_RESULT_SOURCE_REMAPPED:
  //       /* Owner ID got remapped, so add under the new key. */
  //       printf("    remapped %p -> %p\n", ui_state_item.key.owner_id, new_ident.owner_id);
  //       remapped_ui_state.add_new(new_ident, ui_state_item.value);
  //       break;
  //     case IDRemapperApplyResult::ID_REMAP_RESULT_SOURCE_NOT_MAPPABLE:
  //     case IDRemapperApplyResult::ID_REMAP_RESULT_SOURCE_UNASSIGNED:
  //       /* Don't add this to the new UI state map. */
  //       printf("    dropped %p\n", ui_state_item.key.owner_id);
  //       break;
  //   }
  // }
  // sact->runtime->channel_states = std::move(remapped_ui_state);
  // printf("\033[95maction_id_remap: done remapping, left with %ld channel states\033[0m\n",
  //        sact->runtime->channel_states.size());
}

static void action_foreach_id(SpaceLink *space_link, LibraryForeachIDData *data)
{
  SpaceAction *sact = reinterpret_cast<SpaceAction *>(space_link);
  const int data_flags = BKE_lib_query_foreachid_process_flags_get(data);
  const bool is_readonly = (data_flags & IDWALK_READONLY) != 0;

  /* NOTE: Could be deduplicated with the #bDopeSheet handling of #SpaceNla and #SpaceGraph. */
  BKE_LIB_FOREACHID_PROCESS_ID(data, sact->ads.source, IDWALK_CB_DIRECT_WEAK_LINK);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, sact->ads.filter_grp, IDWALK_CB_DIRECT_WEAK_LINK);

  if (!is_readonly) {
    /* Force recalc of list of channels, potentially updating the active action while we're
     * at it (as it can only be updated that way) #28962. */
    sact->runtime->flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
  }

  if (!BLI_listbase_is_empty(&sact->channel_ui_states)) {
    for (const SpaceActionChannelUIState &state : sact->channel_ui_states) {
      BKE_LIB_FOREACHID_PROCESS_ID(data, state.channel_owner_id, IDWALK_CB_DIRECT_WEAK_LINK);
    }
  }
}

static int action_space_subtype_get(ScrArea *area)
{
  SpaceAction *sact = area->spacedata.first_as<SpaceAction>();
  return sact->mode == SACTCONT_TIMELINE ? SACTCONT_TIMELINE : SACTCONT_DOPESHEET;
}

static void action_space_subtype_set(ScrArea *area, int value)
{
  SpaceAction *sact = area->spacedata.first_as<SpaceAction>();
  if (value == SACTCONT_TIMELINE) {
    /* Switching to the timeline. Remember what the current mode of the dope sheet is. */
    if (sact->mode != SACTCONT_TIMELINE) {
      sact->mode_prev = sact->mode;
    }
    sact->mode = SACTCONT_TIMELINE;
  }
  else {
    /* Switching to the 'Dope Sheet' editor, so switch to the last-used mode. Unless that was
     * Timeline, don't use the 'subtype' switch to go back to that; if the user wanted that, we'd
     * be in the `if` case above.  */
    sact->mode = (sact->mode_prev == SACTCONT_TIMELINE) ? SACTCONT_DOPESHEET :
                                                          eAnimEdit_Context(sact->mode_prev);
  }
}

static void action_space_subtype_item_extend(bContext * /*C*/,
                                             EnumPropertyItem **item,
                                             int *totitem)
{
  RNA_enum_items_add(item, totitem, rna_enum_space_action_mode_items);
}

static StringRefNull action_space_name_get(const ScrArea *area)
{
  SpaceAction *sact = area->spacedata.first_as<SpaceAction>();
  const int index = max_ii(0, RNA_enum_from_value(rna_enum_space_action_mode_items, sact->mode));
  const EnumPropertyItem item = rna_enum_space_action_mode_items[index];
  return item.name;
}

static int action_space_icon_get(const ScrArea *area)
{
  SpaceAction *sact = area->spacedata.first_as<SpaceAction>();
  const int index = max_ii(0, RNA_enum_from_value(rna_enum_space_action_mode_items, sact->mode));
  const EnumPropertyItem item = rna_enum_space_action_mode_items[index];
  return item.icon;
}

static void action_space_blend_read_data(BlendDataReader *reader, SpaceLink *sl)
{
  SpaceAction *saction = reinterpret_cast<SpaceAction *>(sl);
  saction->runtime = MEM_new<SpaceAction_Runtime>(__func__);

  /* Read the UI state. This will be converted to runtime data in
   * #action_space_blend_read_after_liblink() below. */
  BLO_read_struct_list(reader, SpaceActionChannelUIState, &saction->channel_ui_states);
  for (SpaceActionChannelUIState &state : saction->channel_ui_states) {
    BLO_read_string(reader, &state.source_rna_path);
  }
}

static void action_space_blend_read_after_liblink(BlendLibReader * /*reader*/,
                                                  ID * /*parent_id*/,
                                                  SpaceLink *sl)
{
  using namespace ed::animfilter;
  SpaceAction *saction = reinterpret_cast<SpaceAction *>(sl);

  for (SpaceActionChannelUIState &state : saction->channel_ui_states) {
    /* Create the UI state map key. */
    ChannelIdentifier chan_ident;
    if (state.channel_owner_id) {
      BKE_lib_libblock_session_uid_ensure(state.channel_owner_id);
      chan_ident.owner_id_type = state.channel_owner_id->id_type();
      chan_ident.owner_session_uid = state.channel_owner_id->session_uid;
    }
    if (state.source_rna_path) {
      chan_ident.source_rna_path =
          ParsedSourcePath::from_string(state.source_rna_path).value_or({});
    }

    /* Create the UI state map value. */
    const SAction_AnimationChannelState ui_state{
        bool(state.is_collapsed),
        state.last_used,
    };

    /* The original map ensured uniqueness, but the ListBase written to DNA does not. So better to
     * use add() rather than add_new(). Also newer Blenders may write more information than what is
     * loaded here, and that missing data may cause channel identifier collissions, so better
     * handle those without crashing. */
    saction->runtime->channel_states.add(chan_ident, ui_state);

    /* Discard the RNA path, as BLI_freelistN() below won't do that. */
    MEM_delete(state.source_rna_path);
    state.source_rna_path = nullptr;
  }

  BLI_freelistN(&saction->channel_ui_states);
}

static void action_space_blend_write(ScreenBlendWriteData &screen_writer, SpaceLink *sl)
{
  BlendWriter *writer = screen_writer.writer;
  if (writer->is_undo()) {
    /* Undo steps don't need the conversion of the runtime UI state to DNA. The screen/space data
     * is not included in the memfile undo, so even when it would be written here, it would never
     * be read anyway. */
    writer->write_struct_cast<SpaceAction>(sl, [&](BlendStructWriter<SpaceAction> &struct_writer) {
      struct_writer.shallow_data.runtime = {};
    });
    return;
  }

  /* TODO: trim the array down to whatever many items we want to keep. */

  /* Allocate memory for the UI state info in DNA. */
  const SpaceAction &space_action = *reinterpret_cast<SpaceAction *>(sl);
  const int64_t num_channel_states = space_action.runtime->channel_states.size();
  ResourceScope &scope = screen_writer.scope;
  auto &ui_state_array = scope.construct<Array<SpaceActionChannelUIState>>(num_channel_states);
  auto &rna_paths = scope.construct<Array<std::string>>(num_channel_states);

  IDNameLib_Map *id_map = writer->idmap();
  BLI_assert_msg(id_map, "Write without bmain, writing UI state will not be possible");

  writer->write_struct_cast<SpaceAction>(sl, [&](BlendStructWriter<SpaceAction> &struct_writer) {
    SpaceAction &for_writing = struct_writer.shallow_data;
    BLI_assert_msg(for_writing.channel_ui_states.is_empty(),
                   "Previous writes should have cleaned up after themselves");

    int64_t array_index = 0;
    for (const auto key_value : for_writing.runtime->channel_states.items()) {
      SpaceActionChannelUIState *state = &ui_state_array[array_index];

      ID *owner_id = BKE_main_idmap_lookup_uid(id_map, key_value.key.owner_session_uid);
      if (!owner_id) {
        /* Stale entry, skip it. */
        continue;
      }

      /* Store the UI state map key. */
      state->channel_owner_id = owner_id;
      if (!key_value.key.source_rna_path.items.is_empty()) {
        rna_paths[array_index] = key_value.key.source_rna_path.to_string();
        state->source_rna_path = rna_paths[array_index].c_str();
      }
      else {
        state->source_rna_path = nullptr;
      }

      /* Store the UI state map value. */
      state->is_collapsed = key_value.value.is_collapsed;
      state->last_used = key_value.value.last_used;

      BLI_addtail(&for_writing.channel_ui_states, state);
      array_index++;
    }

    /* Actually write the structs in the listbase. */
    for (SpaceActionChannelUIState &state : for_writing.channel_ui_states) {
      screen_writer.writer->write_struct(&state);
      if (state.source_rna_path) {
        screen_writer.writer->write_string(state.source_rna_path);
      }
    }

    /* Prevent writing runtime data to the blend file. */
    for_writing.runtime = {};
  });
}

ed::animfilter::SAction_AnimationChannelState &SpaceAction_Runtime::channel_state_for_update(
    const ed::animfilter::ChannelIdentifier &channel_key)
{
  ed::animfilter::SAction_AnimationChannelState &channel_state =
      this->channel_states.lookup_or_add_default(channel_key);
  channel_state.last_used = this->last_used;
  return channel_state;
}

void ED_spacetype_action()
{
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_ACTION;
  STRNCPY_UTF8(st->name, "Action");

  st->create = action_create;
  st->free = action_free;
  st->init = action_init;
  st->duplicate = action_duplicate;
  st->operatortypes = action_operatortypes;
  st->keymap = action_keymap;
  st->listener = action_listener;
  st->refresh = action_refresh;
  st->draw_pre = action_draw_pre;
  st->id_remap = action_id_remap;
  st->foreach_id = action_foreach_id;
  st->space_subtype_item_extend = action_space_subtype_item_extend;
  st->space_subtype_get = action_space_subtype_get;
  st->space_subtype_set = action_space_subtype_set;
  st->space_name_get = action_space_name_get;
  st->space_icon_get = action_space_icon_get;
  st->blend_read_data = action_space_blend_read_data;
  st->blend_read_after_liblink = action_space_blend_read_after_liblink;
  st->blend_write = action_space_blend_write;

  /* regions: main window */
  art = MEM_new_zeroed<ARegionType>("spacetype action region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = action_main_region_init;
  art->draw = action_main_region_draw;
  art->draw_overlay = action_main_region_draw_overlay;
  art->listener = action_main_region_listener;
  art->message_subscribe = saction_main_region_message_subscribe;
  art->keymapflag = ED_KEYMAP_GIZMO | ED_KEYMAP_VIEW2D | ED_KEYMAP_ANIMATION | ED_KEYMAP_FRAMES;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_new_zeroed<ARegionType>("spacetype action region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;

  art->init = action_header_region_init;
  art->draw = action_header_region_draw;
  art->listener = action_header_region_listener;

  BLI_addhead(&st->regiontypes, art);

  /* regions: footer */
  art = MEM_new_zeroed<ARegionType>("spacetype action region");
  art->regionid = RGN_TYPE_FOOTER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FOOTER | ED_KEYMAP_FRAMES;
  art->init = action_header_region_init;
  art->poll = action_region_poll_hide_in_timeline;
  art->draw = action_header_region_draw;
  art->listener = action_footer_region_listener;

  BLI_addhead(&st->regiontypes, art);

  /* regions: channels */
  art = MEM_new_zeroed<ARegionType>("spacetype action region");
  art->regionid = RGN_TYPE_CHANNELS;
  art->prefsizex = 200;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES;

  art->init = action_channel_region_init;
  art->draw = action_channel_region_draw;
  art->listener = action_channel_region_listener;
  art->message_subscribe = saction_channel_region_message_subscribe;

  BLI_addhead(&st->regiontypes, art);

  /* regions: UI buttons */
  art = MEM_new_zeroed<ARegionType>("spacetype action region");
  art->regionid = RGN_TYPE_UI;
  art->flag = ARegionTypeFlag::UsePanelCategoriesSearch;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->listener = action_region_listener;
  art->init = action_buttons_area_init;
  art->draw = action_buttons_area_draw;
  art->poll = action_region_poll_hide_in_timeline;

  BLI_addhead(&st->regiontypes, art);

  action_buttons_register(art);

  art = ui::ED_area_type_hud(st->spaceid);
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(std::move(st));
}

/** \} */

}  // namespace blender
