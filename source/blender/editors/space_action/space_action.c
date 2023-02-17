/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup spaction
 */

#include <stdio.h>
#include <string.h>

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_lib_remap.h"
#include "BKE_nla.h"
#include "BKE_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_markers.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_time_scrub_ui.h"

#include "BLO_read_write.h"

#include "action_intern.h" /* own include */

/* ******************** default callbacks for action space ***************** */

static SpaceLink *action_create(const ScrArea *area, const Scene *scene)
{
  SpaceAction *saction;
  ARegion *region;

  saction = MEM_callocN(sizeof(SpaceAction), "initaction");
  saction->spacetype = SPACE_ACTION;

  saction->autosnap = SACTSNAP_FRAME;
  saction->mode = SACTCONT_DOPESHEET;
  saction->mode_prev = SACTCONT_DOPESHEET;
  saction->flag = SACTION_SHOW_INTERPOLATION | SACTION_SHOW_MARKERS;

  saction->ads.filterflag |= ADS_FILTER_SUMMARY;

  /* enable all cache display */
  saction->cache_display |= TIME_CACHE_DISPLAY;
  saction->cache_display |= (TIME_CACHE_SOFTBODY | TIME_CACHE_PARTICLES);
  saction->cache_display |= (TIME_CACHE_CLOTH | TIME_CACHE_SMOKE | TIME_CACHE_DYNAMICPAINT);
  saction->cache_display |= TIME_CACHE_RIGIDBODY;

  /* header */
  region = MEM_callocN(sizeof(ARegion), "header for action");

  BLI_addtail(&saction->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* channel list region */
  region = MEM_callocN(sizeof(ARegion), "channel region for action");
  BLI_addtail(&saction->regionbase, region);
  region->regiontype = RGN_TYPE_CHANNELS;
  region->alignment = RGN_ALIGN_LEFT;

  /* only need to set scroll settings, as this will use 'listview' v2d configuration */
  region->v2d.scroll = V2D_SCROLL_BOTTOM;
  region->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

  /* ui buttons */
  region = MEM_callocN(sizeof(ARegion), "buttons region for action");

  BLI_addtail(&saction->regionbase, region);
  region->regiontype = RGN_TYPE_UI;
  region->alignment = RGN_ALIGN_RIGHT;

  /* main region */
  region = MEM_callocN(sizeof(ARegion), "main region for action");

  BLI_addtail(&saction->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  region->v2d.tot.xmin = (float)(scene->r.sfra - 10);
  region->v2d.tot.ymin = (float)(-area->winy) / 3.0f;
  region->v2d.tot.xmax = (float)(scene->r.efra + 10);
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

  return (SpaceLink *)saction;
}

/* not spacelink itself */
static void action_free(SpaceLink *UNUSED(sl))
{
  //  SpaceAction *saction = (SpaceAction *) sl;
}

/* spacetype; init callback */
static void action_init(struct wmWindowManager *UNUSED(wm), ScrArea *area)
{
  SpaceAction *saction = area->spacedata.first;
  saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
}

static SpaceLink *action_duplicate(SpaceLink *sl)
{
  SpaceAction *sactionn = MEM_dupallocN(sl);

  memset(&sactionn->runtime, 0x0, sizeof(sactionn->runtime));

  /* clear or remove stuff from old */

  return (SpaceLink *)sactionn;
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_CUSTOM, region->winx, region->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "Dopesheet", SPACE_ACTION, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);
  keymap = WM_keymap_ensure(wm->defaultconf, "Dopesheet Generic", SPACE_ACTION, 0);
  WM_event_add_keymap_handler(&region->handlers, keymap);
}

static void action_main_region_draw(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  SpaceAction *saction = CTX_wm_space_action(C);
  Scene *scene = CTX_data_scene(C);
  Object *obact = CTX_data_active_object(C);
  bAnimContext ac;
  View2D *v2d = &region->v2d;
  short marker_flag = 0;

  UI_view2d_view_ortho(v2d);

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);

  UI_view2d_view_ortho(v2d);

  /* time grid */
  UI_view2d_draw_lines_x__discrete_frames_or_seconds(
      v2d, scene, saction->flag & SACTION_DRAWTIME, true);

  ED_region_draw_cb_draw(C, region, REGION_DRAW_PRE_VIEW);

  /* start and end frame */
  ANIM_draw_framerange(scene, v2d);

  /* Draw the manually set intended playback frame range highlight in the Action editor. */
  if (ELEM(saction->mode, SACTCONT_ACTION, SACTCONT_SHAPEKEY) && saction->action) {
    AnimData *adt = ED_actedit_animdata_from_context(C, NULL);

    ANIM_draw_action_framerange(adt, saction->action, v2d, -FLT_MAX, FLT_MAX);
  }

  /* data */
  if (ANIM_animdata_get_context(C, &ac)) {
    draw_channel_strips(&ac, saction, region);
  }

  /* markers */
  UI_view2d_view_orthoSpecial(region, v2d, 1);

  marker_flag = ((ac.markers && (ac.markers != &ac.scene->markers)) ? DRAW_MARKERS_LOCAL : 0) |
                DRAW_MARKERS_MARGIN;

  if (saction->flag & SACTION_SHOW_MARKERS) {
    ED_markers_draw(C, marker_flag);
  }

  /* caches */
  if (saction->mode == SACTCONT_TIMELINE) {
    timeline_draw_cache(saction, obact, scene);
  }

  /* preview range */
  UI_view2d_view_ortho(v2d);
  ANIM_draw_previewrange(C, v2d, 0);

  /* callback */
  UI_view2d_view_ortho(v2d);
  ED_region_draw_cb_draw(C, region, REGION_DRAW_POST_VIEW);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* gizmos */
  WM_gizmomap_draw(region->gizmo_map, C, WM_GIZMOMAP_DRAWSTEP_2D);

  /* scrubbing region */
  ED_time_scrub_draw(region, scene, saction->flag & SACTION_DRAWTIME, true);
}

static void action_main_region_draw_overlay(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  const SpaceAction *saction = CTX_wm_space_action(C);
  const Scene *scene = CTX_data_scene(C);
  View2D *v2d = &region->v2d;

  /* scrubbing region */
  ED_time_scrub_draw_current_frame(region, scene, saction->flag & SACTION_DRAWTIME);

  /* scrollers */
  UI_view2d_scrollers_draw(v2d, NULL);
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_channel_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  /* ensure the 2d view sync works - main region has bottom scroller */
  region->v2d.scroll = V2D_SCROLL_BOTTOM;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_LIST, region->winx, region->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "Animation Channels", 0, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Dopesheet Generic", SPACE_ACTION, 0);
  WM_event_add_keymap_handler(&region->handlers, keymap);
}

static void action_channel_region_draw(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  bAnimContext ac;
  View2D *v2d = &region->v2d;

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);

  UI_view2d_view_ortho(v2d);

  /* data */
  if (ANIM_animdata_get_context(C, &ac)) {
    draw_channel_names((bContext *)C, &ac, region);
  }

  /* channel filter next to scrubbing area */
  ED_time_scrub_channel_search_draw(C, region, ac.ads);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* no scrollers here */
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_header_region_init(wmWindowManager *UNUSED(wm), ARegion *region)
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
      ED_region_tag_redraw(region);
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_OB_ACTIVE:
        case ND_FRAME:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_KEYS:
          ED_region_tag_redraw(region);
          break;
        case ND_MODIFIER:
          if (wmn->action == NA_RENAME) {
            ED_region_tag_redraw(region);
          }
          break;
      }
      break;
    case NC_GPENCIL:
      if (ELEM(wmn->action, NA_RENAME, NA_SELECTED)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(region);
      }
      break;
    default:
      if (wmn->data == ND_KEYS) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

static void saction_channel_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
{
  struct wmMsgBus *mbus = params->message_bus;
  bScreen *screen = params->screen;
  ScrArea *area = params->area;
  ARegion *region = params->region;

  PointerRNA ptr;
  RNA_pointer_create(&screen->id, &RNA_SpaceDopeSheetEditor, area->spacedata.first, &ptr);

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
      .owner = region,
      .user_data = region,
      .notify = ED_region_do_msg_notify_tag_redraw,
  };

  /* All dopesheet filter settings, etc. affect the drawing of this editor,
   * also same applies for all animation-related datatypes that may appear here,
   * so just whitelist the entire structs for updates
   */
  {
    wmMsgParams_RNA msg_key_params = {{0}};
    StructRNA *type_array[] = {
        &RNA_DopeSheet, /* dopesheet filters */

        &RNA_ActionGroup, /* channel groups */

        &RNA_FCurve, /* F-Curve */
        &RNA_Keyframe,
        &RNA_FCurveSample,

        &RNA_GreasePencil, /* Grease Pencil */
        &RNA_GPencilLayer,
        &RNA_GPencilFrame,
    };

    for (int i = 0; i < ARRAY_SIZE(type_array); i++) {
      msg_key_params.ptr.type = type_array[i];
      WM_msg_subscribe_rna_params(
          mbus, &msg_key_params, &msg_sub_value_region_tag_redraw, __func__);
    }
  }
}

static void action_clamp_scroll(ARegion *region)
{
  View2D *v2d = &region->v2d;
  const float cur_height_y = BLI_rctf_size_y(&v2d->cur);

  if (BLI_rctf_size_y(&v2d->cur) > BLI_rctf_size_y(&v2d->tot)) {
    v2d->cur.ymin = -cur_height_y;
    v2d->cur.ymax = 0;
  }
  else if (v2d->cur.ymin < v2d->tot.ymin) {
    v2d->cur.ymin = v2d->tot.ymin;
    v2d->cur.ymax = v2d->cur.ymin + cur_height_y;
  }
}

static void action_main_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_ANIMATION:
      ED_region_tag_redraw(region);
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_RENDER_OPTIONS:
        case ND_OB_ACTIVE:
        case ND_FRAME:
        case ND_FRAME_RANGE:
        case ND_MARKERS:
          ED_region_tag_redraw(region);
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
        case ND_KEYS:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_NODE:
      switch (wmn->action) {
        case NA_EDITED:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCREEN:
      if (ELEM(wmn->data, ND_LAYER)) {
        ED_region_tag_redraw(region);
      }
      break;
    default:
      if (wmn->data == ND_KEYS) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

static void saction_main_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
{
  struct wmMsgBus *mbus = params->message_bus;
  Scene *scene = params->scene;
  bScreen *screen = params->screen;
  ScrArea *area = params->area;
  ARegion *region = params->region;

  PointerRNA ptr;
  RNA_pointer_create(&screen->id, &RNA_SpaceDopeSheetEditor, area->spacedata.first, &ptr);

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
      .owner = region,
      .user_data = region,
      .notify = ED_region_do_msg_notify_tag_redraw,
  };

  /* Timeline depends on scene properties. */
  {
    bool use_preview = (scene->r.flag & SCER_PRV_RANGE);
    const PropertyRNA *props[] = {
        use_preview ? &rna_Scene_frame_preview_start : &rna_Scene_frame_start,
        use_preview ? &rna_Scene_frame_preview_end : &rna_Scene_frame_end,
        &rna_Scene_use_preview_range,
        &rna_Scene_frame_current,
    };

    PointerRNA idptr;
    RNA_id_pointer_create(&scene->id, &idptr);

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
  SpaceAction *saction = (SpaceAction *)area->spacedata.first;

  /* context changes */
  switch (wmn->category) {
    case NC_GPENCIL:
      /* only handle these events for containers in which GPencil frames are displayed */
      if (ELEM(saction->mode, SACTCONT_GPENCIL, SACTCONT_DOPESHEET, SACTCONT_TIMELINE)) {
        if (wmn->action == NA_EDITED) {
          ED_area_tag_redraw(area);
        }
        else if (wmn->action == NA_SELECTED) {
          saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(area);
        }
      }
      break;
    case NC_ANIMATION:
      /* For NLA tweak-mode enter/exit, need complete refresh. */
      if (wmn->data == ND_NLA_ACTCHANGE) {
        saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
        ED_area_tag_refresh(area);
      }
      /* Auto-color only really needs to change when channels are added/removed,
       * or previously hidden stuff appears
       * (assume for now that if just adding these works, that will be fine).
       */
      else if (((wmn->data == ND_KEYFRAME) && ELEM(wmn->action, NA_ADDED, NA_REMOVED)) ||
               ((wmn->data == ND_ANIMCHAN) && (wmn->action != NA_SELECTED))) {
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
            saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
            ED_area_tag_refresh(area);
          }
          break;
        case ND_OB_ACTIVE:
        case ND_OB_SELECT:
          /* Selection changed, so force refresh to flush
           * (needs flag set to do syncing). */
          saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(area);
          break;
        case ND_RENDER_RESULT:
          ED_area_tag_redraw(area);
          break;
        case ND_FRAME_RANGE:
          LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
            if (region->regiontype == RGN_TYPE_WINDOW) {
              Scene *scene = wmn->reference;
              region->v2d.tot.xmin = (float)(scene->r.sfra - 4);
              region->v2d.tot.xmax = (float)(scene->r.efra + 4);
              break;
            }
          }
          break;
        default:
          if (saction->mode != SACTCONT_TIMELINE) {
            /* Just redrawing the view will do. */
            ED_area_tag_redraw(area);
          }
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_SELECT: /* Selection changed, so force refresh to flush
                              * (needs flag set to do syncing). */
        case ND_BONE_ACTIVE:
          saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(area);
          break;
        case ND_TRANSFORM:
          /* moving object shouldn't need to redraw action */
          break;
        case ND_POINTCACHE:
        case ND_MODIFIER:
        case ND_PARTICLE:
          /* only needed in timeline mode */
          if (saction->mode == SACTCONT_TIMELINE) {
            ED_area_tag_refresh(area);
            ED_area_tag_redraw(area);
          }
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
        saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
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
          saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(area);
          break;
      }
      break;
    case NC_WINDOW:
      if (saction->runtime.flag & SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC) {
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
  ScrArea *area = params->area;
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;
  SpaceAction *saction = (SpaceAction *)area->spacedata.first;

  /* context changes */
  switch (wmn->category) {
    case NC_SCREEN:
      if (saction->mode == SACTCONT_TIMELINE) {
        if (wmn->data == ND_ANIMPLAY) {
          ED_region_tag_redraw(region);
        }
      }
      break;
    case NC_SCENE:
      if (saction->mode == SACTCONT_TIMELINE) {
        switch (wmn->data) {
          case ND_RENDER_RESULT:
          case ND_OB_SELECT:
          case ND_FRAME:
          case ND_FRAME_RANGE:
          case ND_KEYINGSET:
          case ND_RENDER_OPTIONS:
            ED_region_tag_redraw(region);
            break;
        }
      }
      else {
        switch (wmn->data) {
          case ND_OB_ACTIVE:
            ED_region_tag_redraw(region);
            break;
        }
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_ANIMATION:
      switch (wmn->data) {
        case ND_ANIMCHAN: /* set of visible animchannels changed */
          /* NOTE: for now, this should usually just mean that the filters changed
           *       It may be better if we had a dedicated flag for that though
           */
          ED_region_tag_redraw(region);
          break;

        case ND_KEYFRAME: /* new keyframed added -> active action may have changed */
          // saction->flag |= SACTION_TEMP_NEEDCHANSYNC;
          ED_region_tag_redraw(region);
          break;
      }
      break;
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_buttons_area_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(wm->defaultconf, "Dopesheet Generic", SPACE_ACTION, 0);
  WM_event_add_keymap_handler(&region->handlers, keymap);
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
      ED_region_tag_redraw(region);
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_OB_ACTIVE:
        case ND_FRAME:
        case ND_MARKERS:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_KEYS:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    default:
      if (wmn->data == ND_KEYS) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

static void action_refresh(const bContext *C, ScrArea *area)
{
  SpaceAction *saction = (SpaceAction *)area->spacedata.first;

  /* Update the state of the animchannels in response to changes from the data they represent
   * NOTE: the temp flag is used to indicate when this needs to be done,
   * and will be cleared once handled. */
  if (saction->runtime.flag & SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC) {
    ARegion *region;

    /* Perform syncing of channel state incl. selection
     * Active action setting also occurs here
     * (as part of anim channel filtering in anim_filter.c). */
    ANIM_sync_animchannels_to_data(C);
    saction->runtime.flag &= ~SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;

    /* Tag everything for redraw
     * - Regions (such as header) need to be manually tagged for redraw too
     *   or else they don't update #28962.
     */
    ED_area_tag_redraw(area);
    for (region = area->regionbase.first; region; region = region->next) {
      ED_region_tag_redraw(region);
    }
  }

  /* region updates? */
  /* XXX re-sizing y-extents of tot should go here? */
}

static void action_id_remap(ScrArea *UNUSED(area),
                            SpaceLink *slink,
                            const struct IDRemapper *mappings)
{
  SpaceAction *sact = (SpaceAction *)slink;

  BKE_id_remapper_apply(mappings, (ID **)&sact->action, ID_REMAP_APPLY_DEFAULT);
  BKE_id_remapper_apply(mappings, (ID **)&sact->ads.filter_grp, ID_REMAP_APPLY_DEFAULT);
  BKE_id_remapper_apply(mappings, &sact->ads.source, ID_REMAP_APPLY_DEFAULT);
}

/**
 * \note Used for splitting out a subset of modes is more involved,
 * The previous non-timeline mode is stored so switching back to the
 * dope-sheet doesn't always reset the sub-mode.
 */
static int action_space_subtype_get(ScrArea *area)
{
  SpaceAction *sact = area->spacedata.first;
  return sact->mode == SACTCONT_TIMELINE ? SACTCONT_TIMELINE : SACTCONT_DOPESHEET;
}

static void action_space_subtype_set(ScrArea *area, int value)
{
  SpaceAction *sact = area->spacedata.first;
  if (value == SACTCONT_TIMELINE) {
    if (sact->mode != SACTCONT_TIMELINE) {
      sact->mode_prev = sact->mode;
    }
    sact->mode = value;
  }
  else {
    sact->mode = sact->mode_prev;
  }
}

static void action_space_subtype_item_extend(bContext *UNUSED(C),
                                             EnumPropertyItem **item,
                                             int *totitem)
{
  RNA_enum_items_add(item, totitem, rna_enum_space_action_mode_items);
}

static void action_blend_read_data(BlendDataReader *UNUSED(reader), SpaceLink *sl)
{
  SpaceAction *saction = (SpaceAction *)sl;
  memset(&saction->runtime, 0x0, sizeof(saction->runtime));
}

static void action_blend_read_lib(BlendLibReader *reader, ID *parent_id, SpaceLink *sl)
{
  SpaceAction *saction = (SpaceAction *)sl;
  bDopeSheet *ads = &saction->ads;

  if (ads) {
    BLO_read_id_address(reader, parent_id->lib, &ads->source);
    BLO_read_id_address(reader, parent_id->lib, &ads->filter_grp);
  }

  BLO_read_id_address(reader, parent_id->lib, &saction->action);
}

static void action_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  BLO_write_struct(writer, SpaceAction, sl);
}

static void action_main_region_view2d_changed(const bContext *UNUSED(C), ARegion *region)
{
  /* V2D_KEEPTOT_STRICT cannot be used to clamp scrolling
   * because it also clamps the x-axis to 0. */
  action_clamp_scroll(region);
}

void ED_spacetype_action(void)
{
  SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype action");
  ARegionType *art;

  st->spaceid = SPACE_ACTION;
  STRNCPY(st->name, "Action");

  st->create = action_create;
  st->free = action_free;
  st->init = action_init;
  st->duplicate = action_duplicate;
  st->operatortypes = action_operatortypes;
  st->keymap = action_keymap;
  st->listener = action_listener;
  st->refresh = action_refresh;
  st->id_remap = action_id_remap;
  st->space_subtype_item_extend = action_space_subtype_item_extend;
  st->space_subtype_get = action_space_subtype_get;
  st->space_subtype_set = action_space_subtype_set;
  st->blend_read_data = action_blend_read_data;
  st->blend_read_lib = action_blend_read_lib;
  st->blend_write = action_blend_write;

  /* regions: main window */
  art = MEM_callocN(sizeof(ARegionType), "spacetype action region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = action_main_region_init;
  art->draw = action_main_region_draw;
  art->draw_overlay = action_main_region_draw_overlay;
  art->listener = action_main_region_listener;
  art->message_subscribe = saction_main_region_message_subscribe;
  art->on_view2d_changed = action_main_region_view2d_changed;
  art->keymapflag = ED_KEYMAP_GIZMO | ED_KEYMAP_VIEW2D | ED_KEYMAP_ANIMATION | ED_KEYMAP_FRAMES;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN(sizeof(ARegionType), "spacetype action region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;

  art->init = action_header_region_init;
  art->draw = action_header_region_draw;
  art->listener = action_header_region_listener;

  BLI_addhead(&st->regiontypes, art);

  /* regions: channels */
  art = MEM_callocN(sizeof(ARegionType), "spacetype action region");
  art->regionid = RGN_TYPE_CHANNELS;
  art->prefsizex = 200;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES;

  art->init = action_channel_region_init;
  art->draw = action_channel_region_draw;
  art->listener = action_channel_region_listener;
  art->message_subscribe = saction_channel_region_message_subscribe;

  BLI_addhead(&st->regiontypes, art);

  /* regions: UI buttons */
  art = MEM_callocN(sizeof(ARegionType), "spacetype action region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI;
  art->listener = action_region_listener;
  art->init = action_buttons_area_init;
  art->draw = action_buttons_area_draw;

  BLI_addhead(&st->regiontypes, art);

  action_buttons_register(art);

  art = ED_area_type_hud(st->spaceid);
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);
}
