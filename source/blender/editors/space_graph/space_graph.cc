/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spgraph
 */

#include <cstdio>
#include <cstring>

#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_screen.hh"

#include "ED_anim_api.hh"
#include "ED_markers.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_time_scrub_ui.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "BLO_read_write.hh"

#include "graph_intern.hh" /* own include */

/* ******************** default callbacks for ipo space ***************** */

static SpaceLink *graph_create(const ScrArea * /*area*/, const Scene *scene)
{
  ARegion *region;
  SpaceGraph *sipo;

  /* Graph Editor - general stuff */
  sipo = MEM_callocN<SpaceGraph>("init graphedit");
  sipo->spacetype = SPACE_GRAPH;

  /* allocate DopeSheet data for Graph Editor */
  sipo->ads = MEM_callocN<bDopeSheet>("GraphEdit DopeSheet");
  sipo->ads->source = (ID *)scene;

  /* settings for making it easier by default to just see what you're interested in tweaking */
  sipo->ads->filterflag |= ADS_FILTER_ONLYSEL;
  sipo->flag |= SIPO_SHOW_MARKERS;

  /* header */
  region = BKE_area_region_new();

  BLI_addtail(&sipo->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* footer */
  region = BKE_area_region_new();

  BLI_addtail(&sipo->regionbase, region);
  region->regiontype = RGN_TYPE_FOOTER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM;
  region->flag = RGN_FLAG_HIDDEN;

  /* channels */
  region = BKE_area_region_new();

  BLI_addtail(&sipo->regionbase, region);
  region->regiontype = RGN_TYPE_CHANNELS;
  region->alignment = RGN_ALIGN_LEFT;

  region->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);

  /* ui buttons */
  region = BKE_area_region_new();

  BLI_addtail(&sipo->regionbase, region);
  region->regiontype = RGN_TYPE_UI;
  region->alignment = RGN_ALIGN_RIGHT;

  /* main region */
  region = BKE_area_region_new();

  BLI_addtail(&sipo->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  region->v2d.tot.xmin = 0.0f;
  region->v2d.tot.ymin = float(scene->r.sfra) - 10.0f;
  region->v2d.tot.xmax = float(scene->r.efra);
  region->v2d.tot.ymax = 10.0f;

  region->v2d.cur = region->v2d.tot;

  region->v2d.min[0] = FLT_MIN;
  region->v2d.min[1] = FLT_MIN;

  region->v2d.max[0] = MAXFRAMEF;
  region->v2d.max[1] = FLT_MAX;

  region->v2d.scroll = (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
  region->v2d.scroll |= (V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HANDLES);

  region->v2d.keeptot = 0;

  return (SpaceLink *)sipo;
}

/* Doesn't free the space-link itself. */
static void graph_free(SpaceLink *sl)
{
  SpaceGraph *si = (SpaceGraph *)sl;

  if (si->ads) {
    BLI_freelistN(&si->ads->chanbase);
    MEM_freeN(si->ads);
  }

  if (si->runtime.ghost_curves.first) {
    BKE_fcurves_free(&si->runtime.ghost_curves);
  }
}

/* spacetype; init callback */
static void graph_init(wmWindowManager *wm, ScrArea *area)
{
  SpaceGraph *sipo = (SpaceGraph *)area->spacedata.first;

  /* Init dope-sheet if non-existent (i.e. for old files). */
  if (sipo->ads == nullptr) {
    wmWindow *win = WM_window_find_by_area(wm, area);
    sipo->ads = MEM_callocN<bDopeSheet>("GraphEdit DopeSheet");
    sipo->ads->source = win ? (ID *)WM_window_get_active_scene(win) : nullptr;
  }

  /* force immediate init of any invalid F-Curve colors */
  /* XXX: but, don't do SIPO_TEMP_NEEDCHANSYNC (i.e. channel select state sync)
   * as this is run on each region resize; setting this here will cause selection
   * state to be lost on area/region resizing. #35744.
   */
  ED_area_tag_refresh(area);
}

static SpaceLink *graph_duplicate(SpaceLink *sl)
{
  SpaceGraph *sipon = static_cast<SpaceGraph *>(MEM_dupallocN(sl));

  sipon->runtime = SpaceGraph_Runtime{};

  /* clear or remove stuff from old */
  BLI_duplicatelist(&sipon->runtime.ghost_curves, &((SpaceGraph *)sl)->runtime.ghost_curves);
  sipon->ads = static_cast<bDopeSheet *>(MEM_dupallocN(sipon->ads));

  return (SpaceLink *)sipon;
}

/* add handlers, stuff you only do once or on area/region changes */
static void graph_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_CUSTOM, region->winx, region->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Graph Editor", SPACE_GRAPH, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_poll(
      &region->runtime->handlers, keymap, WM_event_handler_region_v2d_mask_no_marker_poll);
  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Graph Editor Generic", SPACE_GRAPH, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
}

/* Draw a darker area above 1 and below -1. */
static void draw_normalization_borders(Scene *scene, View2D *v2d)
{
  GPU_blend(GPU_BLEND_ALPHA);

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(
      format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColorShadeAlpha(TH_BACK, -25, -180);

  if (v2d->cur.ymax >= 1) {
    immRectf(pos, scene->r.sfra, 1, scene->r.efra, v2d->cur.ymax);
  }
  if (v2d->cur.ymin <= -1) {
    immRectf(pos, scene->r.sfra, v2d->cur.ymin, scene->r.efra, -1);
  }

  GPU_blend(GPU_BLEND_NONE);
  immUnbindProgram();
}

static void graph_main_region_draw(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  SpaceGraph *sipo = CTX_wm_space_graph(C);
  Scene *scene = CTX_data_scene(C);
  bAnimContext ac;
  View2D *v2d = &region->v2d;

  const int min_height = UI_ANIM_MINY;

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);

  UI_view2d_view_ortho(v2d);

  /* In driver mode, both X and Y axes are in the same units as the driven property, and so the
   * grid size should be independent of the scene's frame rate. */
  constexpr int driver_step = 10;
  /* grid */
  bool display_seconds = (sipo->mode == SIPO_MODE_ANIMATION) && (sipo->flag & SIPO_DRAWTIME);
  if (region->winy > min_height) {
    if (sipo->mode == SIPO_MODE_DRIVERS) {
      UI_view2d_draw_lines_x__values(v2d, driver_step);
    }
    else {
      UI_view2d_draw_lines_x__frames_or_seconds(v2d, scene, display_seconds);
    }
    UI_view2d_draw_lines_y__values(v2d, 10);
  }

  ED_region_draw_cb_draw(C, region, REGION_DRAW_PRE_VIEW);

  /* start and end frame (in F-Curve mode only) */
  if (sipo->mode != SIPO_MODE_DRIVERS && region->winy > min_height) {
    ANIM_draw_framerange(scene, v2d);
  }

  if (sipo->mode == SIPO_MODE_ANIMATION && (sipo->flag & SIPO_NORMALIZE)) {
    draw_normalization_borders(scene, v2d);
  }

  /* draw data */
  if (ANIM_animdata_get_context(C, &ac)) {
    /* draw ghost curves */
    graph_draw_ghost_curves(&ac, sipo, region);

    /* draw curves twice - unselected, then selected, so that the are fewer occlusion problems */
    graph_draw_curves(&ac, sipo, region, 0);
    graph_draw_curves(&ac, sipo, region, 1);

    /* XXX(ton): the slow way to set tot rect... but for nice sliders needed. */
    /* Excluding handles from the calculation to save performance. This cuts the time it takes for
     * this function to run in half which is a major performance bottleneck on heavy scenes. */
    get_graph_keyframe_extents(
        &ac, &v2d->tot.xmin, &v2d->tot.xmax, &v2d->tot.ymin, &v2d->tot.ymax, false, false);
    /* extra offset so that these items are visible */
    v2d->tot.xmin -= 10.0f;
    v2d->tot.xmax += 10.0f;
  }

  if ((sipo->flag & SIPO_NODRAWCURSOR) == 0) {
    uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", blender::gpu::VertAttrType::SFLOAT_32_32);

    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

    /* horizontal component of value-cursor (value line before the current frame line) */
    float y = sipo->cursorVal;

    /* Draw a line to indicate the cursor value. */
    immUniformThemeColorShadeAlpha(TH_CFRAME, -10, -50);
    GPU_blend(GPU_BLEND_ALPHA);
    GPU_line_width(2.0);

    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, v2d->cur.xmin, y);
    immVertex2f(pos, v2d->cur.xmax, y);
    immEnd();

    GPU_blend(GPU_BLEND_NONE);

    /* Vertical component of the cursor. */
    if (sipo->mode == SIPO_MODE_DRIVERS) {
      /* cursor x-value */
      float x = sipo->cursorTime;

      /* to help differentiate this from the current frame,
       * draw slightly darker like the horizontal one */
      immUniformThemeColorShadeAlpha(TH_CFRAME, -40, -50);
      GPU_blend(GPU_BLEND_ALPHA);
      GPU_line_width(2.0);

      immBegin(GPU_PRIM_LINES, 2);
      immVertex2f(pos, x, v2d->cur.ymin);
      immVertex2f(pos, x, v2d->cur.ymax);
      immEnd();

      GPU_blend(GPU_BLEND_NONE);
    }

    immUnbindProgram();
  }

  /* markers */
  if (sipo->mode != SIPO_MODE_DRIVERS) {
    UI_view2d_view_orthoSpecial(region, v2d, true);
    int marker_draw_flag = DRAW_MARKERS_MARGIN;
    if (ED_markers_region_visible(CTX_wm_area(C), region)) {
      ED_markers_draw(C, marker_draw_flag);
    }
  }

  /* preview range */
  if (sipo->mode != SIPO_MODE_DRIVERS) {
    UI_view2d_view_ortho(v2d);
    ANIM_draw_previewrange(scene, v2d, 0);
  }

  /* callback */
  UI_view2d_view_ortho(v2d);
  ED_region_draw_cb_draw(C, region, REGION_DRAW_POST_VIEW);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* time-scrubbing */
  int base = round_db_to_int(scene->frames_per_second());
  if (sipo->mode == SIPO_MODE_DRIVERS) {
    base = driver_step;
  }
  ED_time_scrub_draw(region, scene, display_seconds, false, base);
}

static void graph_main_region_draw_overlay(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  const SpaceGraph *sipo = CTX_wm_space_graph(C);
  const bool minimized = (region->winy < UI_ANIM_MINY);

  const Scene *scene = CTX_data_scene(C);
  View2D *v2d = &region->v2d;

  /* Driver Editor's X axis is not time. */
  if (sipo->mode != SIPO_MODE_DRIVERS) {
    /* scrubbing region */
    ED_time_scrub_draw_current_frame(region, scene, sipo->flag & SIPO_DRAWTIME, !minimized);
  }

  if (!minimized) {
    /* scrollers */
    const rcti scroller_mask = ED_time_scrub_clamp_scroller_mask(v2d->mask);
    /* FIXME: args for scrollers depend on the type of data being shown. */
    region->v2d.scroll |= V2D_SCROLL_BOTTOM;
    UI_view2d_scrollers_draw(v2d, &scroller_mask);

    /* scale numbers */
    {
      rcti rect;
      BLI_rcti_init(
          &rect, 0, 15 * UI_SCALE_FAC, 15 * UI_SCALE_FAC, region->winy - UI_TIME_SCRUB_MARGIN_Y);
      UI_view2d_draw_scale_y__values(region, v2d, &rect, TH_SCROLL_TEXT, 10);
    }
  }
  else {
    region->v2d.scroll &= ~V2D_SCROLL_BOTTOM;
  }
}

static void graph_channel_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  /* make sure we keep the hide flags */
  region->v2d.scroll |= V2D_SCROLL_RIGHT;

  /* prevent any noise of past */
  region->v2d.scroll &= ~(V2D_SCROLL_LEFT | V2D_SCROLL_TOP | V2D_SCROLL_BOTTOM);

  region->v2d.scroll |= V2D_SCROLL_HORIZONTAL_HIDE;
  region->v2d.scroll |= V2D_SCROLL_VERTICAL_HIDE;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_LIST, region->winx, region->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Animation Channels", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);
  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Graph Editor Generic", SPACE_GRAPH, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);
}

static void set_v2d_height(View2D *v2d, const size_t item_count)
{
  const int height = ANIM_UI_get_channels_total_height(v2d, item_count);
  v2d->tot.ymin = -height;
  UI_view2d_curRect_clamp_y(v2d);
}

static void graph_channel_region_draw(const bContext *C, ARegion *region)
{
  bAnimContext ac;
  if (!ANIM_animdata_get_context(C, &ac)) {
    return;
  }
  View2D *v2d = &region->v2d;

  /* clear and setup matrix */
  UI_ThemeClearColor(TH_BACK);

  ListBase anim_data = {nullptr, nullptr};
  const eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                                    ANIMFILTER_LIST_CHANNELS | ANIMFILTER_FCURVESONLY);
  const size_t item_count = ANIM_animdata_filter(
      &ac, &anim_data, filter, ac.data, eAnimCont_Types(ac.datatype));
  set_v2d_height(v2d, item_count);
  UI_view2d_view_ortho(v2d);

  /* draw channels */
  graph_draw_channel_names((bContext *)C, &ac, region, anim_data);

  /* channel filter next to scrubbing area */
  ED_time_scrub_channel_search_draw(C, region, ac.ads);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* scrollers */
  UI_view2d_scrollers_draw(v2d, nullptr);

  ANIM_animdata_freelist(&anim_data);
}

/* add handlers, stuff you only do once or on area/region changes */
static void graph_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void graph_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

/* add handlers, stuff you only do once or on area/region changes */
static void graph_buttons_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Graph Editor Generic", SPACE_GRAPH, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);
}

static void graph_buttons_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static void graph_region_listener(const wmRegionListenerParams *params)
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
        case ND_SEQUENCER:
          if (wmn->action == NA_SELECTED) {
            ED_region_tag_redraw(region);
          }
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
    case NC_NODE:
      switch (wmn->action) {
        case NA_EDITED:
        case NA_SELECTED:
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

static void graph_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
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

  /* All dope-sheet filter settings, etc. affect the drawing of this editor,
   * also same applies for all animation-related data-types that may appear here,
   * so just whitelist the entire structs for updates
   */
  {
    wmMsgParams_RNA msg_key_params = {{}};
    StructRNA *type_array[] = {
        &RNA_DopeSheet, /* dope-sheet filters */

        &RNA_ActionGroup, /* channel groups */
        &RNA_FCurve,      /* F-Curve */
        &RNA_Keyframe,
        &RNA_FCurveSample,

        &RNA_FModifier, /* F-Modifiers (XXX: Why can't we just do all subclasses too?) */
        &RNA_FModifierCycles,
        &RNA_FModifierEnvelope,
        &RNA_FModifierEnvelopeControlPoint,
        &RNA_FModifierFunctionGenerator,
        &RNA_FModifierGenerator,
        &RNA_FModifierLimits,
        &RNA_FModifierNoise,
        &RNA_FModifierStepped,
    };

    for (int i = 0; i < ARRAY_SIZE(type_array); i++) {
      msg_key_params.ptr.type = type_array[i];
      WM_msg_subscribe_rna_params(
          mbus, &msg_key_params, &msg_sub_value_region_tag_redraw, __func__);
    }
  }
}

/* editor level listener */
static void graph_listener(const wmSpaceTypeListenerParams *params)
{
  ScrArea *area = params->area;
  const wmNotifier *wmn = params->notifier;
  SpaceGraph *sipo = (SpaceGraph *)area->spacedata.first;

  /* context changes */
  switch (wmn->category) {
    case NC_ANIMATION:
      /* For selection changes of animation data, we can just redraw...
       * otherwise auto-color might need to be done again. */
      if (ELEM(wmn->data, ND_KEYFRAME, ND_ANIMCHAN) && (wmn->action == NA_SELECTED)) {
        ED_area_tag_redraw(area);
      }
      else {
        ED_area_tag_refresh(area);
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_OB_ACTIVE: /* Selection changed, so force refresh to flush
                            * (needs flag set to do syncing). */
        case ND_OB_SELECT:
          sipo->runtime.flag |= SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(area);
          break;

        default: /* just redrawing the view will do */
          ED_area_tag_redraw(area);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_SELECT: /* Selection changed, so force refresh to flush
                              * (needs flag set to do syncing). */
        case ND_BONE_ACTIVE:
          sipo->runtime.flag |= SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(area);
          break;
        case ND_TRANSFORM:
          break; /* Do nothing. */

        default: /* just redrawing the view will do */
          ED_area_tag_redraw(area);
          break;
      }
      break;
    case NC_NODE:
      if (wmn->action == NA_SELECTED) {
        /* selection changed, so force refresh to flush (needs flag set to do syncing) */
        sipo->runtime.flag |= SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC;
        ED_area_tag_refresh(area);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_GRAPH) {
        ED_area_tag_redraw(area);
      }
      break;
    case NC_WINDOW:
      if (sipo->runtime.flag &
          (SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC | SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC_COLOR))
      {
        /* force redraw/refresh after undo/redo - prevents "black curve" problem */
        ED_area_tag_refresh(area);
      }
      break;

#if 0 /* XXX: restore the case below if not enough updates occur... */
    default: {
      if (wmn->data == ND_KEYS) {
        ED_area_tag_redraw(area);
      }
    }
#endif
  }
}

/* Update F-Curve colors */
static void graph_refresh_fcurve_colors(const bContext *C)
{
  bAnimContext ac;

  ListBase anim_data = {nullptr, nullptr};
  bAnimListElem *ale;
  size_t items;
  int filter;
  int i;

  if (ANIM_animdata_get_context(C, &ac) == false) {
    return;
  }

  UI_SetTheme(SPACE_GRAPH, RGN_TYPE_WINDOW);

  /* build list of F-Curves which will be visible as channels in channel-region
   * - we don't include ANIMFILTER_CURVEVISIBLE filter, as that will result in a
   *   mismatch between channel-colors and the drawn curves
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS | ANIMFILTER_FCURVESONLY);
  items = ANIM_animdata_filter(
      &ac, &anim_data, eAnimFilter_Flags(filter), ac.data, eAnimCont_Types(ac.datatype));

  /* loop over F-Curves, assigning colors */
  for (ale = static_cast<bAnimListElem *>(anim_data.first), i = 0; ale; ale = ale->next, i++) {
    BLI_assert_msg(ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE),
                   "Expecting only FCurves when using the ANIMFILTER_FCURVESONLY filter");
    FCurve *fcu = (FCurve *)ale->data;

    /* set color of curve here */
    switch (fcu->color_mode) {
      case FCURVE_COLOR_CUSTOM: {
        /* User has defined a custom color for this curve already
         * (we assume it's not going to cause clashes with text colors),
         * which should be left alone... Nothing needs to be done here.
         */
        break;
      }
      case FCURVE_COLOR_AUTO_RGB: {
        /* F-Curve's array index is automatically mapped to RGB values.
         * This works best of 3-value vectors.
         * TODO: find a way to module the hue so that not all curves have same color...
         */
        float *col = fcu->color;

        switch (fcu->array_index) {
          case 0:
            UI_GetThemeColor3fv(TH_AXIS_X, col);
            break;
          case 1:
            UI_GetThemeColor3fv(TH_AXIS_Y, col);
            break;
          case 2:
            UI_GetThemeColor3fv(TH_AXIS_Z, col);
            break;
          default:
            /* 'unknown' color - bluish so as to not conflict with handles */
            col[0] = 0.3f;
            col[1] = 0.8f;
            col[2] = 1.0f;
            break;
        }
        break;
      }
      case FCURVE_COLOR_AUTO_YRGB: {
        /* Like FCURVE_COLOR_AUTO_RGB, except this is for quaternions... */
        float *col = fcu->color;

        switch (fcu->array_index) {
          case 1:
            UI_GetThemeColor3fv(TH_AXIS_X, col);
            break;
          case 2:
            UI_GetThemeColor3fv(TH_AXIS_Y, col);
            break;
          case 3:
            UI_GetThemeColor3fv(TH_AXIS_Z, col);
            break;

          case 0: {
            UI_GetThemeColor3fv(TH_AXIS_W, col);
            break;
          }

          default:
            /* 'unknown' color - bluish so as to not conflict with handles */
            col[0] = 0.3f;
            col[1] = 0.8f;
            col[2] = 1.0f;
            break;
        }
        break;
      }
      case FCURVE_COLOR_AUTO_RAINBOW:
      default: {
        /* determine color 'automatically' using 'magic function' which uses the given args
         * of current item index + total items to determine some RGB color
         */
        getcolor_fcurve_rainbow(i, items, fcu->color);
        break;
      }
    }
  }

  /* free temp list */
  ANIM_animdata_freelist(&anim_data);
}

static void graph_refresh(const bContext *C, ScrArea *area)
{
  SpaceGraph *sipo = (SpaceGraph *)area->spacedata.first;

  /* updates to data needed depends on Graph Editor mode... */
  switch (sipo->mode) {
    case SIPO_MODE_ANIMATION: /* all animation */
    {
      break;
    }

    case SIPO_MODE_DRIVERS: /* Drivers only. */
    {
      break;
    }
  }

  /* region updates? */
  /* XXX re-sizing y-extents of tot should go here? */

  /* Update the state of the animchannels in response to changes from the data they represent
   * NOTE: the temp flag is used to indicate when this needs to be done,
   * and will be cleared once handled. */
  if (sipo->runtime.flag & SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC) {
    ANIM_sync_animchannels_to_data(C);
    sipo->runtime.flag &= ~SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC;
    ED_area_tag_redraw(area);
  }

  /* We could check 'SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC_COLOR', but color is recalculated anyway. */
  if (sipo->runtime.flag & SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC_COLOR) {
    sipo->runtime.flag &= ~SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC_COLOR;
#if 0 /* Done below. */
    graph_refresh_fcurve_colors(C);
#endif
    ED_area_tag_redraw(area);
  }

  sipo->runtime.flag &= ~(SIPO_RUNTIME_FLAG_TWEAK_HANDLES_LEFT |
                          SIPO_RUNTIME_FLAG_TWEAK_HANDLES_RIGHT);

  /* init/adjust F-Curve colors */
  graph_refresh_fcurve_colors(C);
}

static void graph_id_remap(ScrArea * /*area*/,
                           SpaceLink *slink,
                           const blender::bke::id::IDRemapper &mappings)
{
  SpaceGraph *sgraph = (SpaceGraph *)slink;
  if (!sgraph->ads) {
    return;
  }

  mappings.apply(reinterpret_cast<ID **>(&sgraph->ads->filter_grp), ID_REMAP_APPLY_DEFAULT);
  mappings.apply((&sgraph->ads->source), ID_REMAP_APPLY_DEFAULT);
}

static void graph_foreach_id(SpaceLink *space_link, LibraryForeachIDData *data)
{
  SpaceGraph *sgraph = reinterpret_cast<SpaceGraph *>(space_link);
  const int data_flags = BKE_lib_query_foreachid_process_flags_get(data);
  const bool is_readonly = (data_flags & IDWALK_READONLY) != 0;

  /* NOTE: Could be deduplicated with the #bDopeSheet handling of #SpaceAction and #SpaceNla. */
  if (sgraph->ads == nullptr) {
    return;
  }

  BKE_LIB_FOREACHID_PROCESS_ID(data, sgraph->ads->source, IDWALK_CB_DIRECT_WEAK_LINK);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, sgraph->ads->filter_grp, IDWALK_CB_DIRECT_WEAK_LINK);

  if (!is_readonly) {
    /* Force recalc of list of channels (i.e. including calculating F-Curve colors) to
     * prevent the "black curves" problem post-undo. */
    sgraph->runtime.flag |= SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC_COLOR;
  }
}

static int graph_space_subtype_get(ScrArea *area)
{
  SpaceGraph *sgraph = static_cast<SpaceGraph *>(area->spacedata.first);
  return sgraph->mode;
}

static void graph_space_subtype_set(ScrArea *area, int value)
{
  SpaceGraph *sgraph = static_cast<SpaceGraph *>(area->spacedata.first);
  sgraph->mode = value;
}

static void graph_space_subtype_item_extend(bContext * /*C*/,
                                            EnumPropertyItem **item,
                                            int *totitem)
{
  RNA_enum_items_add(item, totitem, rna_enum_space_graph_mode_items);
}

static blender::StringRefNull graph_space_name_get(const ScrArea *area)
{
  SpaceGraph *sgraph = static_cast<SpaceGraph *>(area->spacedata.first);
  const int index = RNA_enum_from_value(rna_enum_space_graph_mode_items, sgraph->mode);
  const EnumPropertyItem item = rna_enum_space_graph_mode_items[index];
  return item.name;
}

static int graph_space_icon_get(const ScrArea *area)
{
  SpaceGraph *sgraph = static_cast<SpaceGraph *>(area->spacedata.first);
  const int index = RNA_enum_from_value(rna_enum_space_graph_mode_items, sgraph->mode);
  const EnumPropertyItem item = rna_enum_space_graph_mode_items[index];
  return item.icon;
}

static void graph_space_blend_read_data(BlendDataReader *reader, SpaceLink *sl)
{
  SpaceGraph *sipo = (SpaceGraph *)sl;

  BLO_read_struct(reader, bDopeSheet, &sipo->ads);
  sipo->runtime = SpaceGraph_Runtime{};
}

static void graph_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  SpaceGraph *sipo = (SpaceGraph *)sl;
  ListBase tmpGhosts = sipo->runtime.ghost_curves;

  /* temporarily disable ghost curves when saving */
  BLI_listbase_clear(&sipo->runtime.ghost_curves);

  BLO_write_struct(writer, SpaceGraph, sl);
  if (sipo->ads) {
    BLO_write_struct(writer, bDopeSheet, sipo->ads);
  }

  /* Re-enable ghost curves. */
  sipo->runtime.ghost_curves = tmpGhosts;
}

static bool action_region_poll_hide_in_driver_mode(const RegionPollParams *params)
{
  BLI_assert(params->area->spacetype == SPACE_GRAPH);
  const SpaceGraph *sipo = static_cast<const SpaceGraph *>(params->area->spacedata.first);
  return sipo->mode != SIPO_MODE_DRIVERS;
}

void ED_spacetype_ipo()
{
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_GRAPH;
  STRNCPY_UTF8(st->name, "Graph");

  st->create = graph_create;
  st->free = graph_free;
  st->init = graph_init;
  st->duplicate = graph_duplicate;
  st->operatortypes = graphedit_operatortypes;
  st->keymap = graphedit_keymap;
  st->listener = graph_listener;
  st->refresh = graph_refresh;
  st->id_remap = graph_id_remap;
  st->foreach_id = graph_foreach_id;
  st->space_subtype_item_extend = graph_space_subtype_item_extend;
  st->space_subtype_get = graph_space_subtype_get;
  st->space_subtype_set = graph_space_subtype_set;
  st->space_name_get = graph_space_name_get;
  st->space_icon_get = graph_space_icon_get;
  st->blend_read_data = graph_space_blend_read_data;
  st->blend_read_after_liblink = nullptr;
  st->blend_write = graph_space_blend_write;

  /* regions: main window */
  art = MEM_callocN<ARegionType>("spacetype graphedit region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = graph_main_region_init;
  art->draw = graph_main_region_draw;
  art->draw_overlay = graph_main_region_draw_overlay;
  art->listener = graph_region_listener;
  art->message_subscribe = graph_region_message_subscribe;
  art->keymapflag = ED_KEYMAP_VIEW2D | ED_KEYMAP_ANIMATION | ED_KEYMAP_FRAMES;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN<ARegionType>("spacetype graphedit region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
  art->listener = graph_region_listener;
  art->init = graph_header_region_init;
  art->draw = graph_header_region_draw;

  BLI_addhead(&st->regiontypes, art);

  /* regions: footer */
  art = static_cast<ARegionType *>(MEM_callocN(sizeof(ARegionType), "spacetype graphedit region"));
  art->regionid = RGN_TYPE_FOOTER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FOOTER;
  art->init = graph_header_region_init;
  art->draw = graph_header_region_draw;
  art->poll = action_region_poll_hide_in_driver_mode;

  BLI_addhead(&st->regiontypes, art);

  /* regions: channels */
  art = MEM_callocN<ARegionType>("spacetype graphedit region");
  art->regionid = RGN_TYPE_CHANNELS;
  /* 200 is the 'standard', but due to scrollers, we want a bit more to fit the lock icons in */
  art->prefsizex = 200 + V2D_SCROLL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES;
  art->listener = graph_region_listener;
  art->message_subscribe = graph_region_message_subscribe;
  art->init = graph_channel_region_init;
  art->draw = graph_channel_region_draw;

  BLI_addhead(&st->regiontypes, art);

  /* regions: UI buttons */
  art = MEM_callocN<ARegionType>("spacetype graphedit region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->listener = graph_region_listener;
  art->init = graph_buttons_region_init;
  art->draw = graph_buttons_region_draw;

  BLI_addhead(&st->regiontypes, art);

  graph_buttons_register(art);

  art = ED_area_type_hud(st->spaceid);
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(std::move(st));
}
