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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spaction
 */

#include <string.h>
#include <stdio.h>

#include "DNA_action_types.h"
#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_anim_api.h"
#include "ED_markers.h"
#include "ED_time_scrub_ui.h"

#include "action_intern.h" /* own include */
#include "GPU_framebuffer.h"

/* ******************** default callbacks for action space ***************** */

static SpaceLink *action_new(const ScrArea *sa, const Scene *scene)
{
  SpaceAction *saction;
  ARegion *ar;

  saction = MEM_callocN(sizeof(SpaceAction), "initaction");
  saction->spacetype = SPACE_ACTION;

  saction->autosnap = SACTSNAP_FRAME;
  saction->mode = SACTCONT_DOPESHEET;
  saction->mode_prev = SACTCONT_DOPESHEET;
  saction->flag = SACTION_SHOW_INTERPOLATION | SACTION_SHOW_MARKER_LINES;

  saction->ads.filterflag |= ADS_FILTER_SUMMARY;

  /* enable all cache display */
  saction->cache_display |= TIME_CACHE_DISPLAY;
  saction->cache_display |= (TIME_CACHE_SOFTBODY | TIME_CACHE_PARTICLES);
  saction->cache_display |= (TIME_CACHE_CLOTH | TIME_CACHE_SMOKE | TIME_CACHE_DYNAMICPAINT);
  saction->cache_display |= TIME_CACHE_RIGIDBODY;

  /* header */
  ar = MEM_callocN(sizeof(ARegion), "header for action");

  BLI_addtail(&saction->regionbase, ar);
  ar->regiontype = RGN_TYPE_HEADER;
  ar->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* channel list region */
  ar = MEM_callocN(sizeof(ARegion), "channel region for action");
  BLI_addtail(&saction->regionbase, ar);
  ar->regiontype = RGN_TYPE_CHANNELS;
  ar->alignment = RGN_ALIGN_LEFT;

  /* only need to set scroll settings, as this will use 'listview' v2d configuration */
  ar->v2d.scroll = V2D_SCROLL_BOTTOM;
  ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

  /* ui buttons */
  ar = MEM_callocN(sizeof(ARegion), "buttons region for action");

  BLI_addtail(&saction->regionbase, ar);
  ar->regiontype = RGN_TYPE_UI;
  ar->alignment = RGN_ALIGN_RIGHT;
  ar->flag = RGN_FLAG_HIDDEN;

  /* main region */
  ar = MEM_callocN(sizeof(ARegion), "main region for action");

  BLI_addtail(&saction->regionbase, ar);
  ar->regiontype = RGN_TYPE_WINDOW;

  ar->v2d.tot.xmin = (float)(SFRA - 10);
  ar->v2d.tot.ymin = (float)(-sa->winy) / 3.0f;
  ar->v2d.tot.xmax = (float)(EFRA + 10);
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

  return (SpaceLink *)saction;
}

/* not spacelink itself */
static void action_free(SpaceLink *UNUSED(sl))
{
  //  SpaceAction *saction = (SpaceAction *) sl;
}

/* spacetype; init callback */
static void action_init(struct wmWindowManager *UNUSED(wm), ScrArea *sa)
{
  SpaceAction *saction = sa->spacedata.first;
  saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
}

static SpaceLink *action_duplicate(SpaceLink *sl)
{
  SpaceAction *sactionn = MEM_dupallocN(sl);

  /* clear or remove stuff from old */

  return (SpaceLink *)sactionn;
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_main_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "Dopesheet", SPACE_ACTION, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
  keymap = WM_keymap_ensure(wm->defaultconf, "Dopesheet Generic", SPACE_ACTION, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void action_main_region_draw(const bContext *C, ARegion *ar)
{
  /* draw entirely, view changes should be handled here */
  SpaceAction *saction = CTX_wm_space_action(C);
  Scene *scene = CTX_data_scene(C);
  Object *obact = CTX_data_active_object(C);
  bAnimContext ac;
  View2D *v2d = &ar->v2d;
  View2DScrollers *scrollers;
  short marker_flag = 0;
  short cfra_flag = 0;

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);
  GPU_clear(GPU_COLOR_BIT);

  UI_view2d_view_ortho(v2d);

  /* time grid */
  UI_view2d_draw_lines_x__discrete_frames_or_seconds(v2d, scene, saction->flag & SACTION_DRAWTIME);

  ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);

  /* start and end frame */
  ANIM_draw_framerange(scene, v2d);

  /* data */
  if (ANIM_animdata_get_context(C, &ac)) {
    draw_channel_strips(&ac, saction, ar);
  }

  /* current frame */
  if (saction->flag & SACTION_DRAWTIME) {
    cfra_flag |= DRAWCFRA_UNIT_SECONDS;
  }
  ANIM_draw_cfra(C, v2d, cfra_flag);

  /* markers */
  UI_view2d_view_orthoSpecial(ar, v2d, 1);

  marker_flag = ((ac.markers && (ac.markers != &ac.scene->markers)) ? DRAW_MARKERS_LOCAL : 0) |
                DRAW_MARKERS_MARGIN;
  if (saction->flag & SACTION_SHOW_MARKER_LINES) {
    marker_flag |= DRAW_MARKERS_LINES;
  }
  ED_markers_draw(C, marker_flag);

  /* caches */
  if (saction->mode == SACTCONT_TIMELINE) {
    timeline_draw_cache(saction, obact, scene);
  }

  /* preview range */
  UI_view2d_view_ortho(v2d);
  ANIM_draw_previewrange(C, v2d, 0);

  /* callback */
  UI_view2d_view_ortho(v2d);
  ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* scrubbing region */
  ED_time_scrub_draw(ar, scene, saction->flag & SACTION_DRAWTIME, true);

  /* scrollers */
  scrollers = UI_view2d_scrollers_calc(v2d, NULL);
  UI_view2d_scrollers_draw(v2d, scrollers);
  UI_view2d_scrollers_free(scrollers);
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_channel_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  /* ensure the 2d view sync works - main region has bottom scroller */
  ar->v2d.scroll = V2D_SCROLL_BOTTOM;

  UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "Animation Channels", 0, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "Dopesheet Generic", SPACE_ACTION, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void action_channel_region_draw(const bContext *C, ARegion *ar)
{
  /* draw entirely, view changes should be handled here */
  bAnimContext ac;
  View2D *v2d = &ar->v2d;

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);
  GPU_clear(GPU_COLOR_BIT);

  UI_view2d_view_ortho(v2d);

  /* data */
  if (ANIM_animdata_get_context(C, &ac)) {
    draw_channel_names((bContext *)C, &ac, ar);
  }

  /* channel filter next to scrubbing area */
  ED_time_scrub_channel_search_draw(C, ar, ac.ads);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* no scrollers here */
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
  ED_region_header_init(ar);
}

static void action_header_region_draw(const bContext *C, ARegion *ar)
{
  ED_region_header(C, ar);
}

static void action_channel_region_listener(wmWindow *UNUSED(win),
                                           ScrArea *UNUSED(sa),
                                           ARegion *ar,
                                           wmNotifier *wmn,
                                           const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_ANIMATION:
      ED_region_tag_redraw(ar);
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_OB_ACTIVE:
        case ND_FRAME:
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
          if (wmn->action == NA_RENAME) {
            ED_region_tag_redraw(ar);
          }
          break;
      }
      break;
    case NC_GPENCIL:
      if (ELEM(wmn->action, NA_RENAME, NA_SELECTED)) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(ar);
      }
      break;
    default:
      if (wmn->data == ND_KEYS) {
        ED_region_tag_redraw(ar);
      }
      break;
  }
}

static void saction_channel_region_message_subscribe(const struct bContext *UNUSED(C),
                                                     struct WorkSpace *UNUSED(workspace),
                                                     struct Scene *UNUSED(scene),
                                                     struct bScreen *screen,
                                                     struct ScrArea *sa,
                                                     struct ARegion *ar,
                                                     struct wmMsgBus *mbus)
{
  PointerRNA ptr;
  RNA_pointer_create(&screen->id, &RNA_SpaceDopeSheetEditor, sa->spacedata.first, &ptr);

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
      .owner = ar,
      .user_data = ar,
      .notify = ED_region_do_msg_notify_tag_redraw,
  };

  /* All dopesheet filter settings, etc. affect the drawing of this editor,
   * also same applies for all animation-related datatypes that may appear here,
   * so just whitelist the entire structs for updates
   */
  {
    wmMsgParams_RNA msg_key_params = {{{0}}};
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

static void action_main_region_listener(wmWindow *UNUSED(win),
                                        ScrArea *UNUSED(sa),
                                        ARegion *ar,
                                        wmNotifier *wmn,
                                        const Scene *UNUSED(scene))
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
        case ND_FRAME_RANGE:
        case ND_MARKERS:
          ED_region_tag_redraw(ar);
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
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_NODE:
      switch (wmn->action) {
        case NA_EDITED:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_SCREEN:
      if (ELEM(wmn->data, ND_LAYER)) {
        ED_region_tag_redraw(ar);
      }
      break;
    default:
      if (wmn->data == ND_KEYS) {
        ED_region_tag_redraw(ar);
      }
      break;
  }
}

static void saction_main_region_message_subscribe(const struct bContext *C,
                                                  struct WorkSpace *workspace,
                                                  struct Scene *scene,
                                                  struct bScreen *screen,
                                                  struct ScrArea *sa,
                                                  struct ARegion *ar,
                                                  struct wmMsgBus *mbus)
{
  PointerRNA ptr;
  RNA_pointer_create(&screen->id, &RNA_SpaceDopeSheetEditor, sa->spacedata.first, &ptr);

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
      .owner = ar,
      .user_data = ar,
      .notify = ED_region_do_msg_notify_tag_redraw,
  };

  /* Timeline depends on scene properties. */
  {
    bool use_preview = (scene->r.flag & SCER_PRV_RANGE);
    extern PropertyRNA rna_Scene_frame_start;
    extern PropertyRNA rna_Scene_frame_end;
    extern PropertyRNA rna_Scene_frame_preview_start;
    extern PropertyRNA rna_Scene_frame_preview_end;
    extern PropertyRNA rna_Scene_use_preview_range;
    extern PropertyRNA rna_Scene_frame_current;
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
  saction_channel_region_message_subscribe(C, workspace, scene, screen, sa, ar, mbus);
}

/* editor level listener */
static void action_listener(wmWindow *UNUSED(win),
                            ScrArea *sa,
                            wmNotifier *wmn,
                            Scene *UNUSED(scene))
{
  SpaceAction *saction = (SpaceAction *)sa->spacedata.first;

  /* context changes */
  switch (wmn->category) {
    case NC_GPENCIL:
      /* only handle these events in GPencil mode for performance considerations */
      if (saction->mode == SACTCONT_GPENCIL) {
        if (wmn->action == NA_EDITED) {
          ED_area_tag_redraw(sa);
        }
        else if (wmn->action == NA_SELECTED) {
          saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(sa);
        }
      }
      break;
    case NC_ANIMATION:
      /* for NLA tweakmode enter/exit, need complete refresh */
      if (wmn->data == ND_NLA_ACTCHANGE) {
        saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
        ED_area_tag_refresh(sa);
      }
      /* autocolor only really needs to change when channels are added/removed,
       * or previously hidden stuff appears
       * (assume for now that if just adding these works, that will be fine)
       */
      else if (((wmn->data == ND_KEYFRAME) && ELEM(wmn->action, NA_ADDED, NA_REMOVED)) ||
               ((wmn->data == ND_ANIMCHAN) && (wmn->action != NA_SELECTED))) {
        ED_area_tag_refresh(sa);
      }
      /* for simple edits to the curve data though (or just plain selections),
       * a simple redraw should work
       * (see T39851 for an example of how this can go wrong)
       */
      else {
        ED_area_tag_redraw(sa);
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_OB_ACTIVE:
        case ND_OB_SELECT:
          /* Selection changed, so force refresh to flush
           * (needs flag set to do syncing). */
          saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(sa);
          break;
        case ND_RENDER_RESULT:
          ED_area_tag_redraw(sa);
          break;
        case ND_FRAME_RANGE:
          for (ARegion *ar = sa->regionbase.first; ar; ar = ar->next) {
            if (ar->regiontype == RGN_TYPE_WINDOW) {
              Scene *scene = wmn->reference;
              ar->v2d.tot.xmin = (float)(SFRA - 4);
              ar->v2d.tot.xmax = (float)(EFRA + 4);
              break;
            }
          }
          break;
        default:
          if (saction->mode != SACTCONT_TIMELINE) {
            /* Just redrawing the view will do. */
            ED_area_tag_redraw(sa);
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
          ED_area_tag_refresh(sa);
          break;
        case ND_TRANSFORM:
          /* moving object shouldn't need to redraw action */
          break;
        case ND_POINTCACHE:
        case ND_MODIFIER:
        case ND_PARTICLE:
          /* only needed in timeline mode */
          if (saction->mode == SACTCONT_TIMELINE) {
            ED_area_tag_refresh(sa);
            ED_area_tag_redraw(sa);
          }
          break;
        default: /* just redrawing the view will do */
          ED_area_tag_redraw(sa);
          break;
      }
      break;
    case NC_MASK:
      if (saction->mode == SACTCONT_MASK) {
        switch (wmn->data) {
          case ND_DATA:
            ED_area_tag_refresh(sa);
            ED_area_tag_redraw(sa);
            break;
          default: /* just redrawing the view will do */
            ED_area_tag_redraw(sa);
            break;
        }
      }
      break;
    case NC_NODE:
      if (wmn->action == NA_SELECTED) {
        /* selection changed, so force refresh to flush (needs flag set to do syncing) */
        saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
        ED_area_tag_refresh(sa);
      }
      break;
    case NC_SPACE:
      switch (wmn->data) {
        case ND_SPACE_DOPESHEET:
          ED_area_tag_redraw(sa);
          break;
        case ND_SPACE_TIME:
          ED_area_tag_redraw(sa);
          break;
        case ND_SPACE_CHANGED:
          saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(sa);
          break;
      }
      break;
    case NC_WINDOW:
      if (saction->runtime.flag & SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC) {
        /* force redraw/refresh after undo/redo - [#28962] */
        ED_area_tag_refresh(sa);
      }
      break;
    case NC_WM:
      switch (wmn->data) {
        case ND_FILEREAD:
          ED_area_tag_refresh(sa);
          break;
      }
      break;
  }
}

static void action_header_region_listener(
    wmWindow *UNUSED(win), ScrArea *sa, ARegion *ar, wmNotifier *wmn, const Scene *UNUSED(scene))
{
  SpaceAction *saction = (SpaceAction *)sa->spacedata.first;

  /* context changes */
  switch (wmn->category) {
    case NC_SCREEN:
      if (saction->mode == SACTCONT_TIMELINE) {
        if (wmn->data == ND_ANIMPLAY) {
          ED_region_tag_redraw(ar);
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
            ED_region_tag_redraw(ar);
            break;
        }
      }
      else {
        switch (wmn->data) {
          case ND_OB_ACTIVE:
            ED_region_tag_redraw(ar);
            break;
        }
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_ANIMATION:
      switch (wmn->data) {
        case ND_ANIMCHAN: /* set of visible animchannels changed */
          /* NOTE: for now, this should usually just mean that the filters changed
           *       It may be better if we had a dedicated flag for that though
           */
          ED_region_tag_redraw(ar);
          break;

        case ND_KEYFRAME: /* new keyframed added -> active action may have changed */
          // saction->flag |= SACTION_TEMP_NEEDCHANSYNC;
          ED_region_tag_redraw(ar);
          break;
      }
      break;
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void action_buttons_area_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, ar);

  keymap = WM_keymap_ensure(wm->defaultconf, "Dopesheet Generic", SPACE_ACTION, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void action_buttons_area_draw(const bContext *C, ARegion *ar)
{
  ED_region_panels(C, ar);
}

static void action_region_listener(wmWindow *UNUSED(win),
                                   ScrArea *UNUSED(sa),
                                   ARegion *ar,
                                   wmNotifier *wmn,
                                   const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_ANIMATION:
      ED_region_tag_redraw(ar);
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_OB_ACTIVE:
        case ND_FRAME:
        case ND_MARKERS:
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
      }
      break;
    default:
      if (wmn->data == ND_KEYS) {
        ED_region_tag_redraw(ar);
      }
      break;
  }
}

static void action_refresh(const bContext *C, ScrArea *sa)
{
  SpaceAction *saction = (SpaceAction *)sa->spacedata.first;

  /* Update the state of the animchannels in response to changes from the data they represent
   * NOTE: the temp flag is used to indicate when this needs to be done,
   * and will be cleared once handled. */
  if (saction->runtime.flag & SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC) {
    ARegion *ar;

    /* Perform syncing of channel state incl. selection
     * Active action setting also occurs here
     * (as part of anim channel filtering in anim_filter.c). */
    ANIM_sync_animchannels_to_data(C);
    saction->runtime.flag &= ~SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;

    /* Tag everything for redraw
     * - Regions (such as header) need to be manually tagged for redraw too
     *   or else they don't update [#28962]
     */
    ED_area_tag_redraw(sa);
    for (ar = sa->regionbase.first; ar; ar = ar->next) {
      ED_region_tag_redraw(ar);
    }
  }

  /* region updates? */
  // XXX re-sizing y-extents of tot should go here?
}

static void action_id_remap(ScrArea *UNUSED(sa), SpaceLink *slink, ID *old_id, ID *new_id)
{
  SpaceAction *sact = (SpaceAction *)slink;

  if ((ID *)sact->action == old_id) {
    sact->action = (bAction *)new_id;
  }

  if ((ID *)sact->ads.filter_grp == old_id) {
    sact->ads.filter_grp = (Collection *)new_id;
  }
  if ((ID *)sact->ads.source == old_id) {
    sact->ads.source = new_id;
  }
}

/**
 * \note Used for splitting out a subset of modes is more involved,
 * The previous non-timeline mode is stored so switching back to the
 * dope-sheet doesn't always reset the sub-mode.
 */
static int action_space_subtype_get(ScrArea *sa)
{
  SpaceAction *sact = sa->spacedata.first;
  return sact->mode == SACTCONT_TIMELINE ? SACTCONT_TIMELINE : SACTCONT_DOPESHEET;
}

static void action_space_subtype_set(ScrArea *sa, int value)
{
  SpaceAction *sact = sa->spacedata.first;
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

/* only called once, from space/spacetypes.c */
void ED_spacetype_action(void)
{
  SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype action");
  ARegionType *art;

  st->spaceid = SPACE_ACTION;
  strncpy(st->name, "Action", BKE_ST_MAXNAME);

  st->new = action_new;
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

  /* regions: main window */
  art = MEM_callocN(sizeof(ARegionType), "spacetype action region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = action_main_region_init;
  art->draw = action_main_region_draw;
  art->listener = action_main_region_listener;
  art->message_subscribe = saction_main_region_message_subscribe;
  art->keymapflag = ED_KEYMAP_VIEW2D | ED_KEYMAP_ANIMATION | ED_KEYMAP_FRAMES;

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

  BKE_spacetype_register(st);
}
