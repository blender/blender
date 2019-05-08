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
 * \ingroup spgraph
 */

#include <string.h>
#include <stdio.h>

#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_anim_api.h"
#include "ED_markers.h"
#include "ED_scrubbing.h"

#include "GPU_immediate.h"
#include "GPU_state.h"
#include "GPU_framebuffer.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"
#include "UI_interface.h"

#include "graph_intern.h"  // own include

/* ******************** default callbacks for ipo space ***************** */

static SpaceLink *graph_new(const ScrArea *UNUSED(sa), const Scene *scene)
{
  ARegion *ar;
  SpaceGraph *sipo;

  /* Graph Editor - general stuff */
  sipo = MEM_callocN(sizeof(SpaceGraph), "init graphedit");
  sipo->spacetype = SPACE_GRAPH;

  sipo->autosnap = SACTSNAP_FRAME;

  /* allocate DopeSheet data for Graph Editor */
  sipo->ads = MEM_callocN(sizeof(bDopeSheet), "GraphEdit DopeSheet");
  sipo->ads->source = (ID *)scene;

  /* settings for making it easier by default to just see what you're interested in tweaking */
  sipo->ads->filterflag |= ADS_FILTER_ONLYSEL;
  sipo->flag |= SIPO_SELVHANDLESONLY | SIPO_MARKER_LINES;

  /* header */
  ar = MEM_callocN(sizeof(ARegion), "header for graphedit");

  BLI_addtail(&sipo->regionbase, ar);
  ar->regiontype = RGN_TYPE_HEADER;
  ar->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* channels */
  ar = MEM_callocN(sizeof(ARegion), "channels region for graphedit");

  BLI_addtail(&sipo->regionbase, ar);
  ar->regiontype = RGN_TYPE_CHANNELS;
  ar->alignment = RGN_ALIGN_LEFT;

  ar->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);

  /* ui buttons */
  ar = MEM_callocN(sizeof(ARegion), "buttons region for graphedit");

  BLI_addtail(&sipo->regionbase, ar);
  ar->regiontype = RGN_TYPE_UI;
  ar->alignment = RGN_ALIGN_RIGHT;
  ar->flag = RGN_FLAG_HIDDEN;

  /* main region */
  ar = MEM_callocN(sizeof(ARegion), "main region for graphedit");

  BLI_addtail(&sipo->regionbase, ar);
  ar->regiontype = RGN_TYPE_WINDOW;

  ar->v2d.tot.xmin = 0.0f;
  ar->v2d.tot.ymin = (float)scene->r.sfra - 10.0f;
  ar->v2d.tot.xmax = (float)scene->r.efra;
  ar->v2d.tot.ymax = 10.0f;

  ar->v2d.cur = ar->v2d.tot;

  ar->v2d.min[0] = FLT_MIN;
  ar->v2d.min[1] = FLT_MIN;

  ar->v2d.max[0] = MAXFRAMEF;
  ar->v2d.max[1] = FLT_MAX;

  ar->v2d.scroll = (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
  ar->v2d.scroll |= (V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HANDLES);

  ar->v2d.keeptot = 0;

  return (SpaceLink *)sipo;
}

/* not spacelink itself */
static void graph_free(SpaceLink *sl)
{
  SpaceGraph *si = (SpaceGraph *)sl;

  if (si->ads) {
    BLI_freelistN(&si->ads->chanbase);
    MEM_freeN(si->ads);
  }

  if (si->runtime.ghost_curves.first) {
    free_fcurves(&si->runtime.ghost_curves);
  }
}

/* spacetype; init callback */
static void graph_init(struct wmWindowManager *wm, ScrArea *sa)
{
  SpaceGraph *sipo = (SpaceGraph *)sa->spacedata.first;

  /* init dopesheet data if non-existent (i.e. for old files) */
  if (sipo->ads == NULL) {
    sipo->ads = MEM_callocN(sizeof(bDopeSheet), "GraphEdit DopeSheet");
    sipo->ads->source = (ID *)WM_window_get_active_scene(wm->winactive);
  }

  /* force immediate init of any invalid F-Curve colors */
  /* XXX: but, don't do SIPO_TEMP_NEEDCHANSYNC (i.e. channel select state sync)
   * as this is run on each region resize; setting this here will cause selection
   * state to be lost on area/region resizing. [#35744]
   */
  ED_area_tag_refresh(sa);
}

static SpaceLink *graph_duplicate(SpaceLink *sl)
{
  SpaceGraph *sipon = MEM_dupallocN(sl);

  /* clear or remove stuff from old */
  BLI_duplicatelist(&sipon->runtime.ghost_curves, &((SpaceGraph *)sl)->runtime.ghost_curves);
  sipon->ads = MEM_dupallocN(sipon->ads);

  return (SpaceLink *)sipon;
}

/* add handlers, stuff you only do once or on area/region changes */
static void graph_main_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "Graph Editor", SPACE_GRAPH, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
  keymap = WM_keymap_ensure(wm->defaultconf, "Graph Editor Generic", SPACE_GRAPH, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void graph_main_region_draw(const bContext *C, ARegion *ar)
{
  /* draw entirely, view changes should be handled here */
  SpaceGraph *sipo = CTX_wm_space_graph(C);
  Scene *scene = CTX_data_scene(C);
  bAnimContext ac;
  View2D *v2d = &ar->v2d;
  View2DScrollers *scrollers;
  float col[3];
  short cfra_flag = 0;

  /* clear and setup matrix */
  UI_GetThemeColor3fv(TH_BACK, col);
  GPU_clear_color(col[0], col[1], col[2], 0.0);
  GPU_clear(GPU_COLOR_BIT);

  UI_view2d_view_ortho(v2d);

  /* grid */
  bool display_seconds = (sipo->mode == SIPO_MODE_ANIMATION) && (sipo->flag & SIPO_DRAWTIME);
  UI_view2d_draw_lines_x__frames_or_seconds(v2d, scene, display_seconds);
  UI_view2d_draw_lines_y__values(v2d);

  ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);

  /* start and end frame (in F-Curve mode only) */
  if (sipo->mode != SIPO_MODE_DRIVERS) {
    ANIM_draw_framerange(scene, v2d);
  }

  /* draw data */
  if (ANIM_animdata_get_context(C, &ac)) {
    /* draw ghost curves */
    graph_draw_ghost_curves(&ac, sipo, ar);

    /* draw curves twice - unselected, then selected, so that the are fewer occlusion problems */
    graph_draw_curves(&ac, sipo, ar, 0);
    graph_draw_curves(&ac, sipo, ar, 1);

    /* XXX the slow way to set tot rect... but for nice sliders needed (ton) */
    get_graph_keyframe_extents(
        &ac, &v2d->tot.xmin, &v2d->tot.xmax, &v2d->tot.ymin, &v2d->tot.ymax, false, true);
    /* extra offset so that these items are visible */
    v2d->tot.xmin -= 10.0f;
    v2d->tot.xmax += 10.0f;
  }

  if (((sipo->flag & SIPO_NODRAWCURSOR) == 0) || (sipo->mode == SIPO_MODE_DRIVERS)) {
    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    /* horizontal component of value-cursor (value line before the current frame line) */
    if ((sipo->flag & SIPO_NODRAWCURSOR) == 0) {
      float y = sipo->cursorVal;

      /* Draw a green line to indicate the cursor value */
      immUniformThemeColorShadeAlpha(TH_CFRAME, -10, -50);
      GPU_blend(true);
      GPU_line_width(2.0);

      immBegin(GPU_PRIM_LINES, 2);
      immVertex2f(pos, v2d->cur.xmin, y);
      immVertex2f(pos, v2d->cur.xmax, y);
      immEnd();

      GPU_blend(false);
    }

    /* current frame or vertical component of vertical component of the cursor */
    if (sipo->mode == SIPO_MODE_DRIVERS) {
      /* cursor x-value */
      float x = sipo->cursorTime;

      /* to help differentiate this from the current frame,
       * draw slightly darker like the horizontal one */
      immUniformThemeColorShadeAlpha(TH_CFRAME, -40, -50);
      GPU_blend(true);
      GPU_line_width(2.0);

      immBegin(GPU_PRIM_LINES, 2);
      immVertex2f(pos, x, v2d->cur.ymin);
      immVertex2f(pos, x, v2d->cur.ymax);
      immEnd();

      GPU_blend(false);
    }

    immUnbindProgram();
  }

  if (sipo->mode != SIPO_MODE_DRIVERS) {
    /* current frame */
    if (sipo->flag & SIPO_DRAWTIME) {
      cfra_flag |= DRAWCFRA_UNIT_SECONDS;
    }
    ANIM_draw_cfra(C, v2d, cfra_flag);
  }

  /* markers */
  UI_view2d_view_orthoSpecial(ar, v2d, 1);
  int marker_draw_flag = DRAW_MARKERS_MARGIN;
  if (sipo->flag & SIPO_MARKER_LINES) {
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

  /* time-scrubbing */
  ED_scrubbing_draw(ar, scene, display_seconds, false);

  /* scrollers */
  // FIXME: args for scrollers depend on the type of data being shown...
  scrollers = UI_view2d_scrollers_calc(v2d, NULL);
  UI_view2d_scrollers_draw(v2d, scrollers);
  UI_view2d_scrollers_free(scrollers);

  /* scale numbers */
  {
    rcti rect;
    BLI_rcti_init(&rect,
                  0,
                  15 * UI_DPI_FAC,
                  15 * UI_DPI_FAC,
                  UI_DPI_FAC * ar->sizey - UI_SCRUBBING_MARGIN_Y);
    UI_view2d_draw_scale_y__values(ar, v2d, &rect, TH_SCROLL_TEXT);
  }
}

static void graph_channel_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  /* make sure we keep the hide flags */
  ar->v2d.scroll |= V2D_SCROLL_RIGHT;

  /* prevent any noise of past */
  ar->v2d.scroll &= ~(V2D_SCROLL_LEFT | V2D_SCROLL_TOP | V2D_SCROLL_BOTTOM);

  ar->v2d.scroll |= V2D_SCROLL_HORIZONTAL_HIDE;
  ar->v2d.scroll |= V2D_SCROLL_VERTICAL_HIDE;

  UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "Animation Channels", 0, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
  keymap = WM_keymap_ensure(wm->defaultconf, "Graph Editor Generic", SPACE_GRAPH, 0);
  WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void graph_channel_region_draw(const bContext *C, ARegion *ar)
{
  bAnimContext ac;
  View2D *v2d = &ar->v2d;
  View2DScrollers *scrollers;
  float col[3];

  /* clear and setup matrix */
  UI_GetThemeColor3fv(TH_BACK, col);
  GPU_clear_color(col[0], col[1], col[2], 0.0);
  GPU_clear(GPU_COLOR_BIT);

  UI_view2d_view_ortho(v2d);

  /* draw channels */
  if (ANIM_animdata_get_context(C, &ac)) {
    graph_draw_channel_names((bContext *)C, &ac, ar);
  }

  /* channel filter next to scrubbing area */
  ED_channel_search_draw(C, ar, ac.ads);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* scrollers */
  scrollers = UI_view2d_scrollers_calc(v2d, NULL);
  UI_view2d_scrollers_draw(v2d, scrollers);
  UI_view2d_scrollers_free(scrollers);
}

/* add handlers, stuff you only do once or on area/region changes */
static void graph_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
  ED_region_header_init(ar);
}

static void graph_header_region_draw(const bContext *C, ARegion *ar)
{
  ED_region_header(C, ar);
}

/* add handlers, stuff you only do once or on area/region changes */
static void graph_buttons_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, ar);

  keymap = WM_keymap_ensure(wm->defaultconf, "Graph Editor Generic", SPACE_GRAPH, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
}

static void graph_buttons_region_draw(const bContext *C, ARegion *ar)
{
  ED_region_panels(C, ar);
}

static void graph_region_listener(wmWindow *UNUSED(win),
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
        case ND_SEQUENCER:
          if (wmn->action == NA_SELECTED) {
            ED_region_tag_redraw(ar);
          }
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
    case NC_NODE:
      switch (wmn->action) {
        case NA_EDITED:
        case NA_SELECTED:
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

static void graph_region_message_subscribe(const struct bContext *UNUSED(C),
                                           struct WorkSpace *UNUSED(workspace),
                                           struct Scene *scene,
                                           struct bScreen *screen,
                                           struct ScrArea *sa,
                                           struct ARegion *ar,
                                           struct wmMsgBus *mbus)
{
  PointerRNA ptr;
  RNA_pointer_create(&screen->id, &RNA_SpaceGraphEditor, sa->spacedata.first, &ptr);

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

  /* All dopesheet filter settings, etc. affect the drawing of this editor,
   * also same applies for all animation-related datatypes that may appear here,
   * so just whitelist the entire structs for updates
   */
  {
    wmMsgParams_RNA msg_key_params = {{{0}}};
    StructRNA *type_array[] = {
        &RNA_DopeSheet, /* dopesheet filters */

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
        &RNA_FModifierPython,
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
static void graph_listener(wmWindow *UNUSED(win),
                           ScrArea *sa,
                           wmNotifier *wmn,
                           Scene *UNUSED(scene))
{
  SpaceGraph *sipo = (SpaceGraph *)sa->spacedata.first;

  /* context changes */
  switch (wmn->category) {
    case NC_ANIMATION:
      /* for selection changes of animation data, we can just redraw...
       * otherwise autocolor might need to be done again */
      if (ELEM(wmn->data, ND_KEYFRAME, ND_ANIMCHAN) && (wmn->action == NA_SELECTED)) {
        ED_area_tag_redraw(sa);
      }
      else {
        ED_area_tag_refresh(sa);
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_OB_ACTIVE: /* Selection changed, so force refresh to flush
                            * (needs flag set to do syncing). */
        case ND_OB_SELECT:
          sipo->runtime.flag |= SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(sa);
          break;

        default: /* just redrawing the view will do */
          ED_area_tag_redraw(sa);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_BONE_SELECT: /* Selection changed, so force refresh to flush
                              * (needs flag set to do syncing). */
        case ND_BONE_ACTIVE:
          sipo->runtime.flag |= SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC;
          ED_area_tag_refresh(sa);
          break;
        case ND_TRANSFORM:
          break; /*do nothing*/

        default: /* just redrawing the view will do */
          ED_area_tag_redraw(sa);
          break;
      }
      break;
    case NC_NODE:
      if (wmn->action == NA_SELECTED) {
        /* selection changed, so force refresh to flush (needs flag set to do syncing) */
        sipo->runtime.flag |= SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC;
        ED_area_tag_refresh(sa);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_GRAPH) {
        ED_area_tag_redraw(sa);
      }
      break;
    case NC_WINDOW:
      if (sipo->runtime.flag &
          (SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC | SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC_COLOR)) {
        /* force redraw/refresh after undo/redo - prevents "black curve" problem */
        ED_area_tag_refresh(sa);
      }
      break;

      // XXX: restore the case below if not enough updates occur...
      // default:
      //  if (wmn->data == ND_KEYS)
      //      ED_area_tag_redraw(sa);
  }
}

/* Update F-Curve colors */
static void graph_refresh_fcurve_colors(const bContext *C)
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
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
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_NODUPLIS);
  items = ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* loop over F-Curves, assigning colors */
  for (ale = anim_data.first, i = 0; ale; ale = ale->next, i++) {
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
            /* Special Case: "W" channel should be yellowish, so blend X and Y channel colors... */
            float c1[3], c2[3];
            float h1[3], h2[3];
            float hresult[3];

            /* - get colors (rgb) */
            UI_GetThemeColor3fv(TH_AXIS_X, c1);
            UI_GetThemeColor3fv(TH_AXIS_Y, c2);

            /* - perform blending in HSV space (to keep brightness similar) */
            rgb_to_hsv_v(c1, h1);
            rgb_to_hsv_v(c2, h2);

            interp_v3_v3v3(hresult, h1, h2, 0.5f);

            /* - convert back to RGB for display */
            hsv_to_rgb_v(hresult, col);
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

static void graph_refresh(const bContext *C, ScrArea *sa)
{
  SpaceGraph *sipo = (SpaceGraph *)sa->spacedata.first;

  /* updates to data needed depends on Graph Editor mode... */
  switch (sipo->mode) {
    case SIPO_MODE_ANIMATION: /* all animation */
    {
      break;
    }

    case SIPO_MODE_DRIVERS: /* drivers only  */
    {
      break;
    }
  }

  /* region updates? */
  // XXX re-sizing y-extents of tot should go here?

  /* Update the state of the animchannels in response to changes from the data they represent
   * NOTE: the temp flag is used to indicate when this needs to be done,
   * and will be cleared once handled. */
  if (sipo->runtime.flag & SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC) {
    ANIM_sync_animchannels_to_data(C);
    sipo->runtime.flag &= ~SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC;
    ED_area_tag_redraw(sa);
  }

  /* We could check 'SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC_COLOR', but color is recalculated anyway. */
  if (sipo->runtime.flag & SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC_COLOR) {
    sipo->runtime.flag &= ~SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC_COLOR;
#if 0 /* Done below. */
    graph_refresh_fcurve_colors(C);
#endif
    ED_area_tag_redraw(sa);
  }

  /* init/adjust F-Curve colors */
  graph_refresh_fcurve_colors(C);
}

static void graph_id_remap(ScrArea *UNUSED(sa), SpaceLink *slink, ID *old_id, ID *new_id)
{
  SpaceGraph *sgraph = (SpaceGraph *)slink;

  if (sgraph->ads) {
    if ((ID *)sgraph->ads->filter_grp == old_id) {
      sgraph->ads->filter_grp = (Collection *)new_id;
    }
    if ((ID *)sgraph->ads->source == old_id) {
      sgraph->ads->source = new_id;
    }
  }
}

static int graph_space_subtype_get(ScrArea *sa)
{
  SpaceGraph *sgraph = sa->spacedata.first;
  return sgraph->mode;
}

static void graph_space_subtype_set(ScrArea *sa, int value)
{
  SpaceGraph *sgraph = sa->spacedata.first;
  sgraph->mode = value;
}

static void graph_space_subtype_item_extend(bContext *UNUSED(C),
                                            EnumPropertyItem **item,
                                            int *totitem)
{
  RNA_enum_items_add(item, totitem, rna_enum_space_graph_mode_items);
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_ipo(void)
{
  SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype ipo");
  ARegionType *art;

  st->spaceid = SPACE_GRAPH;
  strncpy(st->name, "Graph", BKE_ST_MAXNAME);

  st->new = graph_new;
  st->free = graph_free;
  st->init = graph_init;
  st->duplicate = graph_duplicate;
  st->operatortypes = graphedit_operatortypes;
  st->keymap = graphedit_keymap;
  st->listener = graph_listener;
  st->refresh = graph_refresh;
  st->id_remap = graph_id_remap;
  st->space_subtype_item_extend = graph_space_subtype_item_extend;
  st->space_subtype_get = graph_space_subtype_get;
  st->space_subtype_set = graph_space_subtype_set;

  /* regions: main window */
  art = MEM_callocN(sizeof(ARegionType), "spacetype graphedit region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = graph_main_region_init;
  art->draw = graph_main_region_draw;
  art->listener = graph_region_listener;
  art->message_subscribe = graph_region_message_subscribe;
  art->keymapflag = ED_KEYMAP_VIEW2D | ED_KEYMAP_ANIMATION | ED_KEYMAP_FRAMES;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN(sizeof(ARegionType), "spacetype graphedit region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
  art->listener = graph_region_listener;
  art->init = graph_header_region_init;
  art->draw = graph_header_region_draw;

  BLI_addhead(&st->regiontypes, art);

  /* regions: channels */
  art = MEM_callocN(sizeof(ARegionType), "spacetype graphedit region");
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
  art = MEM_callocN(sizeof(ARegionType), "spacetype graphedit region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = 200;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->listener = graph_region_listener;
  art->init = graph_buttons_region_init;
  art->draw = graph_buttons_region_draw;

  BLI_addhead(&st->regiontypes, art);

  graph_buttons_register(art);

  BKE_spacetype_register(st);
}
