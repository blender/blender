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
 * \ingroup spnla
 */

#include <string.h>
#include <stdio.h>

#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_anim_api.h"
#include "ED_markers.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

#include "RNA_access.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "nla_intern.h" /* own include */
#include "GPU_framebuffer.h"

/* ******************** default callbacks for nla space ***************** */

static SpaceLink *nla_new(const ScrArea *sa, const Scene *scene)
{
  ARegion *ar;
  SpaceNla *snla;

  snla = MEM_callocN(sizeof(SpaceNla), "initnla");
  snla->spacetype = SPACE_NLA;

  /* allocate DopeSheet data for NLA Editor */
  snla->ads = MEM_callocN(sizeof(bDopeSheet), "NlaEdit DopeSheet");
  snla->ads->source = (ID *)scene;

  /* set auto-snapping settings */
  snla->autosnap = SACTSNAP_FRAME;

  /* header */
  ar = MEM_callocN(sizeof(ARegion), "header for nla");

  BLI_addtail(&snla->regionbase, ar);
  ar->regiontype = RGN_TYPE_HEADER;
  ar->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* channel list region */
  ar = MEM_callocN(sizeof(ARegion), "channel list for nla");
  BLI_addtail(&snla->regionbase, ar);
  ar->regiontype = RGN_TYPE_CHANNELS;
  ar->alignment = RGN_ALIGN_LEFT;

  /* only need to set these settings since this will use the 'stack' configuration */
  ar->v2d.scroll = V2D_SCROLL_BOTTOM;
  ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

  /* ui buttons */
  ar = MEM_callocN(sizeof(ARegion), "buttons region for nla");

  BLI_addtail(&snla->regionbase, ar);
  ar->regiontype = RGN_TYPE_UI;
  ar->alignment = RGN_ALIGN_RIGHT;
  ar->flag = RGN_FLAG_HIDDEN;

  /* main region */
  ar = MEM_callocN(sizeof(ARegion), "main region for nla");

  BLI_addtail(&snla->regionbase, ar);
  ar->regiontype = RGN_TYPE_WINDOW;

  ar->v2d.tot.xmin = (float)(SFRA - 10);
  ar->v2d.tot.ymin = (float)(-sa->winy) / 3.0f;
  ar->v2d.tot.xmax = (float)(EFRA + 10);
  ar->v2d.tot.ymax = 0.0f;

  ar->v2d.cur = ar->v2d.tot;

  ar->v2d.min[0] = 0.0f;
  ar->v2d.min[1] = 0.0f;

  ar->v2d.max[0] = MAXFRAMEF;
  ar->v2d.max[1] = 10000.0f;

  ar->v2d.minzoom = 0.01f;
  ar->v2d.maxzoom = 50;
  ar->v2d.scroll = (V2D_SCROLL_BOTTOM | V2D_SCROLL_SCALE_HORIZONTAL);
  ar->v2d.scroll |= (V2D_SCROLL_RIGHT);
  ar->v2d.keepzoom = V2D_LOCKZOOM_Y;
  ar->v2d.keepofs = V2D_KEEPOFS_Y;
  ar->v2d.align = V2D_ALIGN_NO_POS_Y;
  ar->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

  return (SpaceLink *)snla;
}

/* not spacelink itself */
static void nla_free(SpaceLink *sl)
{
  SpaceNla *snla = (SpaceNla *)sl;

  if (snla->ads) {
    BLI_freelistN(&snla->ads->chanbase);
    MEM_freeN(snla->ads);
  }
}

/* spacetype; init callback */
static void nla_init(struct wmWindowManager *wm, ScrArea *sa)
{
  SpaceNla *snla = (SpaceNla *)sa->spacedata.first;

  /* init dopesheet data if non-existent (i.e. for old files) */
  if (snla->ads == NULL) {
    snla->ads = MEM_callocN(sizeof(bDopeSheet), "NlaEdit DopeSheet");
    snla->ads->source = (ID *)WM_window_get_active_scene(wm->winactive);
  }

  ED_area_tag_refresh(sa);
}

static SpaceLink *nla_duplicate(SpaceLink *sl)
{
  SpaceNla *snlan = MEM_dupallocN(sl);

  /* clear or remove stuff from old */
  snlan->ads = MEM_dupallocN(snlan->ads);

  return (SpaceLink *)snlan;
}

/* add handlers, stuff you only do once or on area/region changes */
static void nla_channel_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  /* ensure the 2d view sync works - main region has bottom scroller */
  ar->v2d.scroll = V2D_SCROLL_BOTTOM;

  UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);

  /* own keymap */
  /* own channels map first to override some channel keymaps */
  keymap = WM_keymap_ensure(wm->defaultconf, "NLA Channels", SPACE_NLA, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
  /* now generic channels map for everything else that can apply */
  keymap = WM_keymap_ensure(wm->defaultconf, "Animation Channels", 0, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);

  keymap = WM_keymap_ensure(wm->defaultconf, "NLA Generic", SPACE_NLA, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
}

/* draw entirely, view changes should be handled here */
static void nla_channel_region_draw(const bContext *C, ARegion *ar)
{
  bAnimContext ac;
  View2D *v2d = &ar->v2d;
  View2DScrollers *scrollers;

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);
  GPU_clear(GPU_COLOR_BIT);

  UI_view2d_view_ortho(v2d);

  /* data */
  if (ANIM_animdata_get_context(C, &ac)) {
    draw_nla_channel_list(C, &ac, ar);
  }

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* scrollers */
  scrollers = UI_view2d_scrollers_calc(v2d, NULL);
  UI_view2d_scrollers_draw(v2d, scrollers);
  UI_view2d_scrollers_free(scrollers);
}

/* add handlers, stuff you only do once or on area/region changes */
static void nla_main_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "NLA Editor", SPACE_NLA, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
  keymap = WM_keymap_ensure(wm->defaultconf, "NLA Generic", SPACE_NLA, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void nla_main_region_draw(const bContext *C, ARegion *ar)
{
  /* draw entirely, view changes should be handled here */
  SpaceNla *snla = CTX_wm_space_nla(C);
  Scene *scene = CTX_data_scene(C);
  bAnimContext ac;
  View2D *v2d = &ar->v2d;
  View2DScrollers *scrollers;
  short cfra_flag = 0;

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);
  GPU_clear(GPU_COLOR_BIT);

  UI_view2d_view_ortho(v2d);

  /* time grid */
  UI_view2d_draw_lines_x__discrete_frames_or_seconds(v2d, scene, snla->flag & SNLA_DRAWTIME);

  ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);

  /* start and end frame */
  ANIM_draw_framerange(scene, v2d);

  /* data */
  if (ANIM_animdata_get_context(C, &ac)) {
    /* strips and backdrops */
    draw_nla_main_data(&ac, snla, ar);

    /* text draw cached, in pixelspace now */
    UI_view2d_text_cache_draw(ar);
  }

  UI_view2d_view_ortho(v2d);

  /* current frame */
  if (snla->flag & SNLA_DRAWTIME) {
    cfra_flag |= DRAWCFRA_UNIT_SECONDS;
  }
  ANIM_draw_cfra(C, v2d, cfra_flag);

  /* markers */
  UI_view2d_view_orthoSpecial(ar, v2d, 1);
  int marker_draw_flag = DRAW_MARKERS_MARGIN;
  if (snla->flag & SNLA_SHOW_MARKER_LINES) {
    marker_draw_flag |= DRAW_MARKERS_LINES;
  }
  ED_markers_draw(C, marker_draw_flag);

  /* preview range */
  UI_view2d_view_ortho(v2d);
  ANIM_draw_previewrange(C, v2d, 0);

  /* callback */
  UI_view2d_view_ortho(v2d);
  ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* scrollers */
  scrollers = UI_view2d_scrollers_calc(v2d, NULL);
  UI_view2d_scrollers_draw(v2d, scrollers);
  UI_view2d_scrollers_free(scrollers);

  /* frame numbers */
  UI_view2d_draw_scale_x__discrete_frames_or_seconds(
      ar, v2d, &v2d->hor, scene, snla->flag & SNLA_DRAWTIME);

  /* draw current frame number-indicator on top of scrollers */
  if ((snla->flag & SNLA_NODRAWCFRANUM) == 0) {
    UI_view2d_view_orthoSpecial(ar, v2d, 1);
    ANIM_draw_cfra_number(C, v2d, cfra_flag);
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void nla_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
  ED_region_header_init(ar);
}

static void nla_header_region_draw(const bContext *C, ARegion *ar)
{
  ED_region_header(C, ar);
}

/* add handlers, stuff you only do once or on area/region changes */
static void nla_buttons_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, ar);

  keymap = WM_keymap_ensure(wm->defaultconf, "NLA Generic", SPACE_NLA, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
}

static void nla_buttons_region_draw(const bContext *C, ARegion *ar)
{
  ED_region_panels(C, ar);
}

static void nla_region_listener(wmWindow *UNUSED(win),
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

static void nla_main_region_listener(wmWindow *UNUSED(win),
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
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_KEYS:
        case ND_TRANSFORM:
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

static void nla_main_region_message_subscribe(const struct bContext *UNUSED(C),
                                              struct WorkSpace *UNUSED(workspace),
                                              struct Scene *scene,
                                              struct bScreen *screen,
                                              struct ScrArea *sa,
                                              struct ARegion *ar,
                                              struct wmMsgBus *mbus)
{
  PointerRNA ptr;
  RNA_pointer_create(&screen->id, &RNA_SpaceNLA, sa->spacedata.first, &ptr);

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
}

static void nla_channel_region_listener(wmWindow *UNUSED(win),
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

static void nla_channel_region_message_subscribe(const struct bContext *UNUSED(C),
                                                 struct WorkSpace *UNUSED(workspace),
                                                 struct Scene *UNUSED(scene),
                                                 struct bScreen *screen,
                                                 struct ScrArea *sa,
                                                 struct ARegion *ar,
                                                 struct wmMsgBus *mbus)
{
  PointerRNA ptr;
  RNA_pointer_create(&screen->id, &RNA_SpaceNLA, sa->spacedata.first, &ptr);

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
      .owner = ar,
      .user_data = ar,
      .notify = ED_region_do_msg_notify_tag_redraw,
  };

  /* All dopesheet filter settings, etc. affect the drawing of this editor,
   * so just whitelist the entire struct for updates
   */
  {
    wmMsgParams_RNA msg_key_params = {{{0}}};
    StructRNA *type_array[] = {
        &RNA_DopeSheet,
    };

    for (int i = 0; i < ARRAY_SIZE(type_array); i++) {
      msg_key_params.ptr.type = type_array[i];
      WM_msg_subscribe_rna_params(
          mbus, &msg_key_params, &msg_sub_value_region_tag_redraw, __func__);
    }
  }
}

/* editor level listener */
static void nla_listener(wmWindow *UNUSED(win), ScrArea *sa, wmNotifier *wmn, Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_ANIMATION:
      // TODO: filter specific types of changes?
      ED_area_tag_refresh(sa);
      break;
    case NC_SCENE:
#if 0
      switch (wmn->data) {
        case ND_OB_ACTIVE:
        case ND_OB_SELECT:
          ED_area_tag_refresh(sa);
          break;
      }
#endif
      ED_area_tag_refresh(sa);
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_TRANSFORM:
          /* do nothing */
          break;
        default:
          ED_area_tag_refresh(sa);
          break;
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_NLA) {
        ED_area_tag_redraw(sa);
      }
      break;
  }
}

static void nla_id_remap(ScrArea *UNUSED(sa), SpaceLink *slink, ID *old_id, ID *new_id)
{
  SpaceNla *snla = (SpaceNla *)slink;

  if (snla->ads) {
    if ((ID *)snla->ads->filter_grp == old_id) {
      snla->ads->filter_grp = (Collection *)new_id;
    }
    if ((ID *)snla->ads->source == old_id) {
      snla->ads->source = new_id;
    }
  }
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_nla(void)
{
  SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype nla");
  ARegionType *art;

  st->spaceid = SPACE_NLA;
  strncpy(st->name, "NLA", BKE_ST_MAXNAME);

  st->new = nla_new;
  st->free = nla_free;
  st->init = nla_init;
  st->duplicate = nla_duplicate;
  st->operatortypes = nla_operatortypes;
  st->listener = nla_listener;
  st->keymap = nla_keymap;
  st->id_remap = nla_id_remap;

  /* regions: main window */
  art = MEM_callocN(sizeof(ARegionType), "spacetype nla region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = nla_main_region_init;
  art->draw = nla_main_region_draw;
  art->listener = nla_main_region_listener;
  art->message_subscribe = nla_main_region_message_subscribe;
  art->keymapflag = ED_KEYMAP_VIEW2D | ED_KEYMAP_MARKERS | ED_KEYMAP_ANIMATION | ED_KEYMAP_FRAMES;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN(sizeof(ARegionType), "spacetype nla region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;

  art->init = nla_header_region_init;
  art->draw = nla_header_region_draw;

  BLI_addhead(&st->regiontypes, art);

  /* regions: channels */
  art = MEM_callocN(sizeof(ARegionType), "spacetype nla region");
  art->regionid = RGN_TYPE_CHANNELS;
  art->prefsizex = 200;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES;

  art->init = nla_channel_region_init;
  art->draw = nla_channel_region_draw;
  art->listener = nla_channel_region_listener;
  art->message_subscribe = nla_channel_region_message_subscribe;

  BLI_addhead(&st->regiontypes, art);

  /* regions: UI buttons */
  art = MEM_callocN(sizeof(ARegionType), "spacetype nla region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = 200;
  art->keymapflag = ED_KEYMAP_UI;
  art->listener = nla_region_listener;
  art->init = nla_buttons_region_init;
  art->draw = nla_buttons_region_draw;

  BLI_addhead(&st->regiontypes, art);

  nla_buttons_register(art);

  BKE_spacetype_register(st);
}
