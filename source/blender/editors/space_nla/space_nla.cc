/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnla
 */

#include <cstdio>
#include <cstring>

#include "DNA_collection_types.h"
#include "DNA_scene_types.h"

#include "DNA_screen_types.h"
#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_screen.hh"

#include "ED_anim_api.hh"
#include "ED_markers.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_time_scrub_ui.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_types.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "BLO_read_write.hh"

#include "nla_intern.hh" /* own include */

/* ******************** default callbacks for nla space ***************** */

static SpaceLink *nla_create(const ScrArea *area, const Scene *scene)
{
  ARegion *region;
  SpaceNla *snla;

  snla = MEM_callocN<SpaceNla>("initnla");
  snla->spacetype = SPACE_NLA;

  /* allocate DopeSheet data for NLA Editor */
  snla->ads = MEM_callocN<bDopeSheet>("NlaEdit DopeSheet");
  snla->ads->source = (ID *)(scene);

  /* set auto-snapping settings */
  snla->flag = SNLA_SHOW_MARKERS;

  /* header */
  region = BKE_area_region_new();

  BLI_addtail(&snla->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* footer */
  region = BKE_area_region_new();

  BLI_addtail(&snla->regionbase, region);
  region->regiontype = RGN_TYPE_FOOTER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM;
  region->flag = RGN_FLAG_HIDDEN;

  /* track list region */
  region = BKE_area_region_new();
  BLI_addtail(&snla->regionbase, region);
  region->regiontype = RGN_TYPE_CHANNELS;
  region->alignment = RGN_ALIGN_LEFT;

  /* only need to set these settings since this will use the 'stack' configuration */
  region->v2d.scroll = V2D_SCROLL_BOTTOM;
  region->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

  /* ui buttons */
  region = BKE_area_region_new();

  BLI_addtail(&snla->regionbase, region);
  region->regiontype = RGN_TYPE_UI;
  region->alignment = RGN_ALIGN_RIGHT;

  /* main region */
  region = BKE_area_region_new();

  BLI_addtail(&snla->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  region->v2d.tot.xmin = float(scene->r.sfra - 10);
  region->v2d.tot.ymin = float(-area->winy) / 3.0f;
  region->v2d.tot.xmax = float(scene->r.efra + 10);
  region->v2d.tot.ymax = 0.0f;

  region->v2d.cur = region->v2d.tot;

  region->v2d.min[0] = 0.0f;
  region->v2d.min[1] = 0.0f;

  region->v2d.max[0] = MAXFRAMEF;
  region->v2d.max[1] = 10000.0f;

  region->v2d.minzoom = 0.01f;
  region->v2d.maxzoom = 50;
  region->v2d.scroll = (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
  region->v2d.scroll |= V2D_SCROLL_RIGHT;
  region->v2d.keepzoom = V2D_LOCKZOOM_Y;
  region->v2d.keepofs = V2D_KEEPOFS_Y;
  region->v2d.align = V2D_ALIGN_NO_POS_Y;
  region->v2d.flag = V2D_VIEWSYNC_AREA_VERTICAL;

  return reinterpret_cast<SpaceLink *>(snla);
}

/* Doesn't free the space-link itself. */
static void nla_free(SpaceLink *sl)
{
  SpaceNla *snla = reinterpret_cast<SpaceNla *>(sl);

  if (snla->ads) {
    BLI_freelistN(&snla->ads->chanbase);
    MEM_freeN(snla->ads);
  }
}

/* spacetype; init callback */
static void nla_init(wmWindowManager *wm, ScrArea *area)
{
  SpaceNla *snla = static_cast<SpaceNla *>(area->spacedata.first);

  /* init dope-sheet data if non-existent (i.e. for old files). */
  if (snla->ads == nullptr) {
    snla->ads = MEM_callocN<bDopeSheet>("NlaEdit DopeSheet");
    wmWindow *win = WM_window_find_by_area(wm, area);
    snla->ads->source = win ? reinterpret_cast<ID *>(WM_window_get_active_scene(win)) : nullptr;
  }

  ED_area_tag_refresh(area);
}

static SpaceLink *nla_duplicate(SpaceLink *sl)
{
  SpaceNla *snlan = static_cast<SpaceNla *>(MEM_dupallocN(sl));

  /* clear or remove stuff from old */
  snlan->ads = static_cast<bDopeSheet *>(MEM_dupallocN(snlan->ads));

  return reinterpret_cast<SpaceLink *>(snlan);
}

/* add handlers, stuff you only do once or on area/region changes */
static void nla_track_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  /* ensure the 2d view sync works - main region has bottom scroller */
  region->v2d.scroll = V2D_SCROLL_BOTTOM;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_LIST, region->winx, region->winy);

  /* own keymap */
  /* own tracks map first to override some track keymaps */
  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "NLA Tracks", SPACE_NLA, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_poll(
      &region->runtime->handlers, keymap, WM_event_handler_region_v2d_mask_no_marker_poll);
  /* now generic channels map for everything else that can apply */
  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Animation Channels", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "NLA Generic", SPACE_NLA, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);
}

/* draw entirely, view changes should be handled here */
static void nla_track_region_draw(const bContext *C, ARegion *region)
{
  bAnimContext ac;
  if (!ANIM_animdata_get_context(C, &ac)) {
    return;
  }

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);

  ListBase anim_data = {nullptr, nullptr};

  SpaceNla *snla = reinterpret_cast<SpaceNla *>(ac.sl);
  View2D *v2d = &region->v2d;

  const eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                                    ANIMFILTER_LIST_CHANNELS | ANIMFILTER_FCURVESONLY);
  const size_t item_count = ANIM_animdata_filter(
      &ac, &anim_data, filter, ac.data, eAnimCont_Types(ac.datatype));

  /* Recalculate the height of the track list.
   * Needs to be done before the call to #UI_view2d_view_ortho. */
  int height = NLATRACK_TOT_HEIGHT(&ac, item_count);
  /* Add padding for the collapsed redo panel. */
  height += HEADERY;
  if (!BLI_listbase_is_empty(ED_context_get_markers(C))) {
    height += (UI_MARKER_MARGIN_Y - NLATRACK_STEP(snla));
  }
  v2d->tot.ymin = -height;
  UI_view2d_curRect_clamp_y(v2d);

  UI_view2d_view_ortho(v2d);

  draw_nla_track_list(C, &ac, region, anim_data);

  /* track filter next to scrubbing area */
  ED_time_scrub_channel_search_draw(C, region, ac.ads);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* scrollers */
  if (region->winy > UI_ANIM_MINY) {
    UI_view2d_scrollers_draw(v2d, nullptr);
  }

  ANIM_animdata_freelist(&anim_data);
}

/* add handlers, stuff you only do once or on area/region changes */
static void nla_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_CUSTOM, region->winx, region->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "NLA Editor", SPACE_NLA, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);
  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "NLA Generic", SPACE_NLA, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
}

static void nla_main_region_draw(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  SpaceNla *snla = CTX_wm_space_nla(C);
  Scene *scene = CTX_data_scene(C);
  bAnimContext ac;
  View2D *v2d = &region->v2d;

  const int min_height = UI_ANIM_MINY;

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);

  UI_view2d_view_ortho(v2d);

  /* time grid */
  if (region->winy > min_height) {
    UI_view2d_draw_lines_x__discrete_frames_or_seconds(
        v2d, scene, snla->flag & SNLA_DRAWTIME, true);
  }

  ED_region_draw_cb_draw(C, region, REGION_DRAW_PRE_VIEW);

  /* start and end frame */
  if (region->winy > min_height) {
    ANIM_draw_framerange(scene, v2d);
  }

  /* data */
  if (ANIM_animdata_get_context(C, &ac)) {
    /* strips and backdrops */
    draw_nla_main_data(&ac, snla, region);

    /* Text draw cached, in pixel-space now. */
    UI_view2d_text_cache_draw(region);
  }

  /* markers */
  UI_view2d_view_orthoSpecial(region, v2d, true);
  int marker_draw_flag = DRAW_MARKERS_MARGIN;
  if (ED_markers_region_visible(CTX_wm_area(C), region)) {
    ED_markers_draw(C, marker_draw_flag);
  }

  /* preview range */
  UI_view2d_view_ortho(v2d);
  ANIM_draw_previewrange(scene, v2d, 0);

  /* callback */
  UI_view2d_view_ortho(v2d);
  ED_region_draw_cb_draw(C, region, REGION_DRAW_POST_VIEW);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  const int fps = round_db_to_int(scene->frames_per_second());
  ED_time_scrub_draw(region, scene, snla->flag & SNLA_DRAWTIME, true, fps);
}

static void nla_main_region_draw_overlay(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  const SpaceNla *snla = CTX_wm_space_nla(C);
  const Scene *scene = CTX_data_scene(C);
  View2D *v2d = &region->v2d;

  /* scrubbing region */
  ED_time_scrub_draw_current_frame(
      region, scene, snla->flag & SNLA_DRAWTIME, region->winy >= UI_ANIM_MINY);

  /* scrollers */
  if (region->winy >= UI_ANIM_MINY) {
    UI_view2d_scrollers_draw(v2d, nullptr);
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void nla_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void nla_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

static void nla_footer_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_SCREEN:
      if (wmn->data == ND_ANIMPLAY) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
          ED_region_tag_redraw(region);
          break;
      }
      break;
  }
}

/* add handlers, stuff you only do once or on area/region changes */
static void nla_buttons_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "NLA Generic", SPACE_NLA, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);
}

static void nla_buttons_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static void nla_region_listener(const wmRegionListenerParams *params)
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
        case ND_LAYER_CONTENT:
        case ND_OB_SELECT:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_KEYS:
        case ND_DRAW:
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

static void nla_main_region_listener(const wmRegionListenerParams *params)
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
        case ND_LAYER_CONTENT:
        case ND_OB_SELECT:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_KEYS:
        case ND_TRANSFORM:
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

static void nla_main_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
{
  wmMsgBus *mbus = params->message_bus;
  Scene *scene = params->scene;
  ARegion *region = params->region;

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {};
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
}

static void nla_track_region_listener(const wmRegionListenerParams *params)
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
        case ND_LAYER_CONTENT:
        case ND_FRAME:
        case ND_OB_SELECT:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_KEYS:
        case ND_DRAW:
          ED_region_tag_redraw(region);
          break;
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

static void nla_track_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
{
  wmMsgBus *mbus = params->message_bus;
  ARegion *region = params->region;

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {};
  msg_sub_value_region_tag_redraw.owner = region;
  msg_sub_value_region_tag_redraw.user_data = region;
  msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;

  /* All dope-sheet filter settings, etc. affect the drawing of this editor,
   * so just whitelist the entire struct for updates. */
  {
    wmMsgParams_RNA msg_key_params = {{}};
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
static void nla_listener(const wmSpaceTypeListenerParams *params)
{
  ScrArea *area = params->area;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_ANIMATION:
      /* TODO: filter specific types of changes? */
      ED_area_tag_refresh(area);
      break;
    case NC_SCENE:
#if 0
      switch (wmn->data) {
        case ND_OB_ACTIVE:
        case ND_OB_SELECT:
          ED_area_tag_refresh(area);
          break;
      }
#endif
      ED_area_tag_refresh(area);
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_TRANSFORM:
          /* do nothing */
          break;
        default:
          ED_area_tag_refresh(area);
          break;
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_NLA) {
        ED_area_tag_redraw(area);
      }
      break;
  }
}

static void nla_id_remap(ScrArea * /*area*/,
                         SpaceLink *slink,
                         const blender::bke::id::IDRemapper &mappings)
{
  SpaceNla *snla = reinterpret_cast<SpaceNla *>(slink);

  if (snla->ads == nullptr) {
    return;
  }

  mappings.apply(reinterpret_cast<ID **>(&snla->ads->filter_grp), ID_REMAP_APPLY_DEFAULT);
  mappings.apply((&snla->ads->source), ID_REMAP_APPLY_DEFAULT);
}

static void nla_foreach_id(SpaceLink *space_link, LibraryForeachIDData *data)
{
  SpaceNla *snla = reinterpret_cast<SpaceNla *>(space_link);

  /* NOTE: Could be deduplicated with the #bDopeSheet handling of #SpaceAction and #SpaceGraph. */
  if (snla->ads == nullptr) {
    return;
  }

  BKE_LIB_FOREACHID_PROCESS_ID(data, snla->ads->source, IDWALK_CB_DIRECT_WEAK_LINK);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, snla->ads->filter_grp, IDWALK_CB_DIRECT_WEAK_LINK);
}

static void nla_space_blend_read_data(BlendDataReader *reader, SpaceLink *sl)
{
  SpaceNla *snla = reinterpret_cast<SpaceNla *>(sl);
  BLO_read_struct(reader, bDopeSheet, &snla->ads);
}

static void nla_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  SpaceNla *snla = reinterpret_cast<SpaceNla *>(sl);

  BLO_write_struct(writer, SpaceNla, snla);
  if (snla->ads) {
    BLO_write_struct(writer, bDopeSheet, snla->ads);
  }
}

void ED_spacetype_nla()
{
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_NLA;
  STRNCPY_UTF8(st->name, "NLA");

  st->create = nla_create;
  st->free = nla_free;
  st->init = nla_init;
  st->duplicate = nla_duplicate;
  st->operatortypes = nla_operatortypes;
  st->listener = nla_listener;
  st->keymap = nla_keymap;
  st->id_remap = nla_id_remap;
  st->foreach_id = nla_foreach_id;
  st->blend_read_data = nla_space_blend_read_data;
  st->blend_read_after_liblink = nullptr;
  st->blend_write = nla_space_blend_write;

  /* regions: main window */
  art = MEM_callocN<ARegionType>("spacetype nla region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = nla_main_region_init;
  art->draw = nla_main_region_draw;
  art->draw_overlay = nla_main_region_draw_overlay;
  art->listener = nla_main_region_listener;
  art->message_subscribe = nla_main_region_message_subscribe;
  art->keymapflag = ED_KEYMAP_VIEW2D | ED_KEYMAP_ANIMATION | ED_KEYMAP_FRAMES;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN<ARegionType>("spacetype nla region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;

  art->init = nla_header_region_init;
  art->draw = nla_header_region_draw;

  BLI_addhead(&st->regiontypes, art);

  /* regions: footer */
  art = MEM_callocN<ARegionType>("spacetype nla region");
  art->regionid = RGN_TYPE_FOOTER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FOOTER;

  art->init = nla_header_region_init;
  art->draw = nla_header_region_draw;
  art->listener = nla_footer_region_listener;

  BLI_addhead(&st->regiontypes, art);

  /* regions: tracks */
  art = MEM_callocN<ARegionType>("spacetype nla region");
  art->regionid = RGN_TYPE_CHANNELS;
  art->prefsizex = 200;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES;

  art->init = nla_track_region_init;
  art->draw = nla_track_region_draw;
  art->listener = nla_track_region_listener;
  art->message_subscribe = nla_track_region_message_subscribe;

  BLI_addhead(&st->regiontypes, art);

  /* regions: UI buttons */
  art = MEM_callocN<ARegionType>("spacetype nla region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->listener = nla_region_listener;
  art->init = nla_buttons_region_init;
  art->draw = nla_buttons_region_draw;

  BLI_addhead(&st->regiontypes, art);

  nla_buttons_register(art);

  art = ED_area_type_hud(st->spaceid);
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(std::move(st));
}
