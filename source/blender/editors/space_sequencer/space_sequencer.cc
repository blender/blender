/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include <algorithm>
#include <cmath>
#include <cstring>

#include "DNA_gpencil_legacy_types.h"
#include "DNA_mask_types.h"
#include "DNA_scene_types.h"

#include "GPU_immediate.hh"
#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"

#include "BLF_api.hh"

#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_screen.hh"

#include "IMB_colormanagement.hh"

#include "ED_screen.hh"
#include "ED_sequencer.hh"
#include "ED_space_api.hh"
#include "ED_transform.hh"
#include "ED_util.hh"
#include "ED_view3d_offscreen.hh" /* Only for sequencer view3d drawing callback. */

#include "WM_api.hh"
#include "WM_message.hh"

#include "SEQ_channels.hh"
#include "SEQ_offscreen.hh"
#include "SEQ_preview_cache.hh"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"
#include "SEQ_utils.hh"

#include "UI_interface.hh"
#include "UI_view2d.hh"

#include "BLO_read_write.hh"

/* Own include. */
#include "sequencer_intern.hh"

namespace blender::ed::vse {

/**************************** common state *****************************/

static void sequencer_scopes_tag_refresh(ScrArea *area, const Scene *scene)
{
  SpaceSeq *sseq = (SpaceSeq *)area->spacedata.first;
  sseq->runtime->scopes.cleanup();
  seq::preview_cache_invalidate(const_cast<Scene *>(scene));
}

SpaceSeq_Runtime::~SpaceSeq_Runtime() = default;

/* ******************** default callbacks for sequencer space ***************** */

static SpaceLink *sequencer_create(const ScrArea * /*area*/, const Scene *scene)
{
  ARegion *region;
  SpaceSeq *sseq;

  sseq = MEM_callocN<SpaceSeq>("initsequencer");
  sseq->runtime = MEM_new<SpaceSeq_Runtime>(__func__);
  sseq->spacetype = SPACE_SEQ;
  sseq->chanshown = 0;
  sseq->view = SEQ_VIEW_SEQUENCE;
  sseq->mainb = SEQ_DRAW_IMG_IMBUF;
  sseq->flag = SEQ_USE_ALPHA | SEQ_SHOW_MARKERS | SEQ_ZOOM_TO_FIT | SEQ_SHOW_OVERLAY;
  sseq->preview_overlay.flag = SEQ_PREVIEW_SHOW_GPENCIL | SEQ_PREVIEW_SHOW_OUTLINE_SELECTED;
  sseq->timeline_overlay.flag = SEQ_TIMELINE_SHOW_STRIP_NAME | SEQ_TIMELINE_SHOW_STRIP_SOURCE |
                                SEQ_TIMELINE_SHOW_STRIP_DURATION | SEQ_TIMELINE_SHOW_GRID |
                                SEQ_TIMELINE_SHOW_FCURVES | SEQ_TIMELINE_SHOW_STRIP_COLOR_TAG |
                                SEQ_TIMELINE_SHOW_STRIP_RETIMING | SEQ_TIMELINE_WAVEFORMS_HALF |
                                SEQ_TIMELINE_SHOW_THUMBNAILS;
  sseq->cache_overlay.flag = SEQ_CACHE_SHOW | SEQ_CACHE_SHOW_FINAL_OUT;
  sseq->draw_flag |= SEQ_DRAW_TRANSFORM_PREVIEW;

  /* Header. */
  region = BKE_area_region_new();

  BLI_addtail(&sseq->regionbase, static_cast<void *>(region));
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* Tool header. */
  region = BKE_area_region_new();

  BLI_addtail(&sseq->regionbase, static_cast<void *>(region));
  region->regiontype = RGN_TYPE_TOOL_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;
  region->flag = RGN_FLAG_HIDDEN | RGN_FLAG_HIDDEN_BY_USER;

  /* Footer. */
  region = BKE_area_region_new();

  BLI_addtail(&sseq->regionbase, region);
  region->regiontype = RGN_TYPE_FOOTER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM;

  /* Buttons/list view. */
  region = BKE_area_region_new();

  BLI_addtail(&sseq->regionbase, static_cast<void *>(region));
  region->regiontype = RGN_TYPE_UI;
  region->alignment = RGN_ALIGN_RIGHT;
  region->flag = RGN_FLAG_HIDDEN;

  /* Toolbar. */
  region = BKE_area_region_new();

  BLI_addtail(&sseq->regionbase, static_cast<void *>(region));
  region->regiontype = RGN_TYPE_TOOLS;
  region->alignment = RGN_ALIGN_LEFT;

  /* Channels. */
  region = BKE_area_region_new();

  BLI_addtail(&sseq->regionbase, static_cast<void *>(region));
  region->regiontype = RGN_TYPE_CHANNELS;
  region->alignment = RGN_ALIGN_LEFT;
  region->v2d.flag |= V2D_VIEWSYNC_AREA_VERTICAL;

  /* Preview region. */
  /* NOTE: if you change values here, also change them in sequencer_init_preview_region. */
  region = BKE_area_region_new();
  BLI_addtail(&sseq->regionbase, static_cast<void *>(region));
  region->regiontype = RGN_TYPE_PREVIEW;
  region->alignment = RGN_ALIGN_TOP;
  /* For now, aspect ratio should be maintained, and zoom is clamped within sane default limits. */
  region->v2d.keepzoom = V2D_KEEPASPECT | V2D_KEEPZOOM | V2D_LIMITZOOM;
  region->v2d.minzoom = 0.001f;
  region->v2d.maxzoom = 1000.0f;
  region->v2d.tot.xmin = -960.0f; /* 1920 width centered. */
  region->v2d.tot.ymin = -540.0f; /* 1080 height centered. */
  region->v2d.tot.xmax = 960.0f;
  region->v2d.tot.ymax = 540.0f;
  region->v2d.min[0] = 0.0f;
  region->v2d.min[1] = 0.0f;
  region->v2d.max[0] = 12000.0f;
  region->v2d.max[1] = 12000.0f;
  region->v2d.cur = region->v2d.tot;
  region->v2d.align = V2D_ALIGN_FREE;
  region->v2d.keeptot = V2D_KEEPTOT_FREE;

  /* Main region. */
  region = BKE_area_region_new();

  BLI_addtail(&sseq->regionbase, static_cast<void *>(region));
  region->regiontype = RGN_TYPE_WINDOW;

  /* Seq space goes from (0,8) to (0, efra). */
  region->v2d.tot.xmin = 0.0f;
  region->v2d.tot.ymin = 0.0f;
  region->v2d.tot.xmax = scene->r.efra;
  region->v2d.tot.ymax = 8.5f;

  region->v2d.cur = region->v2d.tot;

  region->v2d.min[0] = 10.0f;
  region->v2d.min[1] = 1.0f;

  region->v2d.max[0] = MAXFRAMEF;
  region->v2d.max[1] = seq::MAX_CHANNELS;

  region->v2d.minzoom = 0.01f;
  region->v2d.maxzoom = 100.0f;

  region->v2d.scroll |= (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
  region->v2d.scroll |= (V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HANDLES);
  region->v2d.keepzoom = V2D_KEEPZOOM;
  region->v2d.keepofs = V2D_KEEPOFS_X | V2D_KEEPOFS_Y;
  region->v2d.keeptot = V2D_KEEPTOT_FREE;
  region->v2d.flag |= V2D_VIEWSYNC_AREA_VERTICAL | V2D_ZOOM_IGNORE_KEEPOFS;
  region->v2d.align = V2D_ALIGN_NO_NEG_Y;

  return (SpaceLink *)sseq;
}

/* Not spacelink itself. */
static void sequencer_free(SpaceLink *sl)
{
  SpaceSeq *sseq = (SpaceSeq *)sl;
  MEM_delete(sseq->runtime);

#if 0
  if (sseq->gpd) {
    BKE_gpencil_free_data(sseq->gpd);
  }
#endif
}

/* Space-type init callback. */
static void sequencer_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static void sequencer_refresh(const bContext *C, ScrArea *area)
{
  const wmWindow *window = CTX_wm_window(C);
  SpaceSeq *sseq = (SpaceSeq *)area->spacedata.first;
  ARegion *region_main = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  ARegion *region_preview = BKE_area_find_region_type(area, RGN_TYPE_PREVIEW);
  bool view_changed = false;

  switch (sseq->view) {
    case SEQ_VIEW_PREVIEW:
      /* Reset scrolling when preview region just appears. */
      if (!(region_preview->v2d.flag & V2D_IS_INIT)) {
        region_preview->v2d.cur = region_preview->v2d.tot;
        /* Only redraw, don't re-init. */
        ED_area_tag_redraw(area);
      }
      if (region_preview->alignment != RGN_ALIGN_NONE) {
        region_preview->alignment = RGN_ALIGN_NONE;
        view_changed = true;
      }
      break;
    case SEQ_VIEW_SEQUENCE_PREVIEW: {
      /* Get available height (without DPI correction). */
      const float height = (area->winy - ED_area_headersize()) / UI_SCALE_FAC;

      /* We reuse hidden region's size, allows to find same layout as before if we just switch
       * between one 'full window' view and the combined one. This gets lost if we switch to both
       * 'full window' views before, though... Better than nothing. */
      if (!(region_preview->v2d.flag & V2D_IS_INIT)) {
        region_preview->v2d.cur = region_preview->v2d.tot;
        region_main->sizey = int(height - region_preview->sizey);
        region_preview->sizey = int(height - region_main->sizey);
        view_changed = true;
      }
      if (region_preview->alignment != RGN_ALIGN_TOP) {
        region_preview->alignment = RGN_ALIGN_TOP;
        view_changed = true;
      }
      /* Final check that both preview and main height are reasonable. */
      if (region_preview->sizey < 10 || region_main->sizey < 10 ||
          region_preview->sizey + region_main->sizey > height)
      {
        region_preview->sizey = roundf(height * 0.4f);
        region_main->sizey = int(height - region_preview->sizey);
        view_changed = true;
      }
      break;
    }
    case SEQ_VIEW_SEQUENCE:
      break;
  }

  if (view_changed) {
    ED_area_init(const_cast<bContext *>(C), window, area);
    ED_area_tag_redraw(area);
  }
}

static SpaceLink *sequencer_duplicate(SpaceLink *sl)
{
  SpaceSeq *sseqn = static_cast<SpaceSeq *>(MEM_dupallocN(sl));
  sseqn->runtime = MEM_new<SpaceSeq_Runtime>(__func__);

  /* Clear or remove stuff from old. */
  // sseq->gpd = gpencil_data_duplicate(sseq->gpd, false);

  return (SpaceLink *)sseqn;
}

static void sequencer_listener(const wmSpaceTypeListenerParams *params)
{
  ScrArea *area = params->area;
  const wmNotifier *wmn = params->notifier;

  /* Context changes. */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
        case ND_SEQUENCER:
          sequencer_scopes_tag_refresh(area, params->scene);
          break;
      }
      break;
    case NC_WINDOW:
    case NC_SPACE:
      if (wmn->data == ND_SPACE_SEQUENCER) {
        sequencer_scopes_tag_refresh(area, params->scene);
      }
      break;
    case NC_GPENCIL:
      if (wmn->data & ND_GPENCIL_EDITMODE) {
        ED_area_tag_redraw(area);
      }
      break;
  }
}

/* DO NOT make this static, this hides the symbol and breaks API generation script. */
extern "C" const char *sequencer_context_dir[]; /* Quiet warning. */
const char *sequencer_context_dir[] = {"edit_mask", "tool_settings", nullptr};

static int /*eContextResult*/ sequencer_context(const bContext *C,
                                                const char *member,
                                                bContextDataResult *result)
{
  Scene *scene = CTX_data_sequencer_scene(C);

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, sequencer_context_dir);

    return CTX_RESULT_OK;
  }
  if (CTX_data_equals(member, "tool_settings")) {
    if (scene) {
      CTX_data_pointer_set(result, &scene->id, &RNA_ToolSettings, scene->toolsettings);
      return CTX_RESULT_OK;
    }
  }
  if (CTX_data_equals(member, "edit_mask")) {
    if (scene) {
      Mask *mask = seq::active_mask_get(scene);
      if (mask) {
        CTX_data_id_pointer_set(result, &mask->id);
      }
      return CTX_RESULT_OK;
    }
  }

  return CTX_RESULT_MEMBER_NOT_FOUND;
}

static void SEQUENCER_GGT_navigate(wmGizmoGroupType *gzgt)
{
  VIEW2D_GGT_navigate_impl(gzgt, "SEQUENCER_GGT_navigate");
}

static void SEQUENCER_GGT_gizmo2d(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Sequencer Transform Gizmo";
  gzgt->idname = "SEQUENCER_GGT_gizmo2d";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP |
                 WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK);

  gzgt->gzmap_params.spaceid = SPACE_SEQ;
  gzgt->gzmap_params.regionid = RGN_TYPE_PREVIEW;

  transform::ED_widgetgroup_gizmo2d_xform_callbacks_set(gzgt);
}

static void SEQUENCER_GGT_gizmo2d_translate(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Sequencer Translate Gizmo";
  gzgt->idname = "SEQUENCER_GGT_gizmo2d_translate";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP |
                 WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK);

  gzgt->gzmap_params.spaceid = SPACE_SEQ;
  gzgt->gzmap_params.regionid = RGN_TYPE_PREVIEW;

  transform::ED_widgetgroup_gizmo2d_xform_no_cage_callbacks_set(gzgt);
}

static void SEQUENCER_GGT_gizmo2d_resize(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Sequencer Transform Gizmo Resize";
  gzgt->idname = "SEQUENCER_GGT_gizmo2d_resize";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP |
                 WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK);

  gzgt->gzmap_params.spaceid = SPACE_SEQ;
  gzgt->gzmap_params.regionid = RGN_TYPE_PREVIEW;

  transform::ED_widgetgroup_gizmo2d_resize_callbacks_set(gzgt);
}

static void SEQUENCER_GGT_gizmo2d_rotate(wmGizmoGroupType *gzgt)
{
  gzgt->name = "Sequencer Transform Gizmo Resize";
  gzgt->idname = "SEQUENCER_GGT_gizmo2d_rotate";

  gzgt->flag |= (WM_GIZMOGROUPTYPE_TOOL_FALLBACK_KEYMAP |
                 WM_GIZMOGROUPTYPE_DELAY_REFRESH_FOR_TWEAK);

  gzgt->gzmap_params.spaceid = SPACE_SEQ;
  gzgt->gzmap_params.regionid = RGN_TYPE_PREVIEW;

  transform::ED_widgetgroup_gizmo2d_rotate_callbacks_set(gzgt);
}

static void sequencer_gizmos()
{
  WM_gizmogrouptype_append(SEQUENCER_GGT_gizmo2d);
  WM_gizmogrouptype_append(SEQUENCER_GGT_gizmo2d_translate);
  WM_gizmogrouptype_append(SEQUENCER_GGT_gizmo2d_resize);
  WM_gizmogrouptype_append(SEQUENCER_GGT_gizmo2d_rotate);

  const wmGizmoMapType_Params params_preview = {SPACE_SEQ, RGN_TYPE_PREVIEW};
  wmGizmoMapType *gzmap_type_preview = WM_gizmomaptype_ensure(&params_preview);
  WM_gizmogrouptype_append_and_link(gzmap_type_preview, SEQUENCER_GGT_navigate);
}

/* *********************** sequencer (main) region ************************ */

static bool sequencer_main_region_poll(const RegionPollParams *params)
{
  const SpaceSeq *sseq = (SpaceSeq *)params->area->spacedata.first;
  return ELEM(sseq->view, SEQ_VIEW_SEQUENCE, SEQ_VIEW_SEQUENCE_PREVIEW);
}

/* Add handlers, stuff you only do once or on area/region changes. */
static void sequencer_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;
  ListBase *lb;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_CUSTOM, region->winx, region->winy);

#if 0
  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Mask Editing", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);
#endif

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Video Sequence Editor", SPACE_SEQ, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  /* Own keymap. */
  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Sequencer", SPACE_SEQ, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_poll(
      &region->runtime->handlers, keymap, WM_event_handler_region_v2d_mask_no_marker_poll);

  /* Add drop boxes. */
  lb = WM_dropboxmap_find("Sequencer", SPACE_SEQ, RGN_TYPE_WINDOW);

  WM_event_add_dropbox_handler(&region->runtime->handlers, lb);
}

/* Strip editing timeline. */
static void sequencer_main_region_draw(const bContext *C, ARegion *region)
{
  draw_timeline_seq(C, region);
}

/* Strip editing timeline. */
static void sequencer_main_region_draw_overlay(const bContext *C, ARegion *region)
{
  draw_timeline_seq_display(C, region);
}

static void sequencer_main_clamp_view(const bContext *C, ARegion *region)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);

  if ((sseq->flag & SEQ_CLAMP_VIEW) == 0) {
    return;
  }

  View2D *v2d = &region->v2d;
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return;
  }

  /* Transformation uses edge panning to move view. Also if smooth view is running, don't apply
   * clamping to prevent overriding this functionality. */
  if (G.moving || v2d->smooth_timer != nullptr) {
    return;
  }

  rctf strip_boundbox;
  /* Initialize default view with 7 channels, that are visible even if empty. */
  seq::timeline_init_boundbox(scene, &strip_boundbox);
  Editing *ed = seq::editing_get(scene);
  if (ed != nullptr) {
    seq::timeline_expand_boundbox(scene, ed->current_strips(), &strip_boundbox);
  }
  /* We need to calculate how much the current view is padded and add this padding to our
   * strip bounding box. Without this, the scrub-bar or other overlays would occlude the
   * displayed strips in the timeline.
   */
  float pad_top, pad_bottom;
  SEQ_get_timeline_region_padding(C, &pad_top, &pad_bottom);
  const float pixel_view_size_y = BLI_rctf_size_y(&v2d->cur) / (BLI_rcti_size_y(&v2d->mask) + 1);
  /* Add padding to be able to scroll the view so that the collapsed redo panel doesn't occlude any
   * strips. */
  float bottom_channel_padding = UI_MARKER_MARGIN_Y * pixel_view_size_y;
  bottom_channel_padding = std::max(bottom_channel_padding, 1.0f);
  /* Add the padding and make sure we have a margin of one channel in each direction. */
  strip_boundbox.ymax += 1.0f + pad_top * pixel_view_size_y;
  strip_boundbox.ymin -= bottom_channel_padding;

  /* If a strip has been deleted, don't move the view automatically, keep current range until it is
   * changed. */
  strip_boundbox.ymax = max_ff(sseq->runtime->timeline_clamp_custom_range, strip_boundbox.ymax);

  rctf view_clamped = v2d->cur;
  float range_y = BLI_rctf_size_y(&view_clamped);
  if (view_clamped.ymax > strip_boundbox.ymax) {
    view_clamped.ymax = strip_boundbox.ymax;
    view_clamped.ymin = max_ff(strip_boundbox.ymin, strip_boundbox.ymax - range_y);
  }
  else if (view_clamped.ymin < strip_boundbox.ymin) {
    view_clamped.ymin = strip_boundbox.ymin;
    view_clamped.ymax = min_ff(strip_boundbox.ymax, strip_boundbox.ymin + range_y);
  }

  v2d->cur = view_clamped;
}

static void sequencer_main_region_clamp_custom_set(const bContext *C, ARegion *region)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  View2D *v2d = &region->v2d;

  if ((v2d->flag & V2D_IS_NAVIGATING) == 0) {
    sseq->runtime->timeline_clamp_custom_range = v2d->cur.ymax;
  }
}

static void sequencer_main_region_layout(const bContext *C, ARegion *region)
{
  sequencer_main_region_clamp_custom_set(C, region);
  sequencer_main_clamp_view(C, region);
}

static void sequencer_main_region_view2d_changed(const bContext *C, ARegion *region)
{
  sequencer_main_region_clamp_custom_set(C, region);
  sequencer_main_clamp_view(C, region);
}

static void sequencer_main_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* Context changes. */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
        case ND_FRAME_RANGE:
        case ND_MARKERS:
        case ND_RENDER_OPTIONS: /* For FPS and FPS Base. */
        case ND_SEQUENCER:
        case ND_RENDER_RESULT:
          ED_region_tag_redraw(region);
          WM_gizmomap_tag_refresh(region->runtime->gizmo_map);
          break;
      }
      break;
    case NC_ANIMATION:
      switch (wmn->data) {
        case ND_KEYFRAME:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_SEQUENCER) {
        ED_region_tag_redraw(region);
        WM_gizmomap_tag_refresh(region->runtime->gizmo_map);
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCREEN:
      if (ELEM(wmn->data, ND_ANIMPLAY)) {
        ED_region_tag_redraw(region);
        WM_gizmomap_tag_refresh(region->runtime->gizmo_map);
      }
      break;
  }
}

static void sequencer_main_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
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

  {
    StructRNA *type_array[] = {
        &RNA_SequenceEditor,

        &RNA_Strip,
        /* Members of 'Strip'. */
        &RNA_StripCrop,
        &RNA_StripTransform,
        &RNA_StripModifier,
        &RNA_StripColorBalanceData,
    };
    wmMsgParams_RNA msg_key_params = {{}};
    for (int i = 0; i < ARRAY_SIZE(type_array); i++) {
      msg_key_params.ptr.type = type_array[i];
      WM_msg_subscribe_rna_params(
          mbus, &msg_key_params, &msg_sub_value_region_tag_redraw, __func__);
    }
  }
}

static bool is_mouse_over_retiming_key(const Scene *scene,
                                       const Strip *strip,
                                       const View2D *v2d,
                                       const ScrArea *area,
                                       float mouse_co_region[2])
{
  const SpaceSeq *sseq = static_cast<SpaceSeq *>(area->spacedata.first);

  if (!seq::retiming_data_is_editable(strip) || !retiming_keys_can_be_displayed(sseq)) {
    return false;
  }

  rctf retiming_keys_box = strip_retiming_keys_box_get(scene, v2d, strip);
  return BLI_rctf_isect_pt_v(&retiming_keys_box, mouse_co_region);
}

static void sequencer_main_cursor(wmWindow *win, ScrArea *area, ARegion *region)
{
  const WorkSpace *workspace = WM_window_get_active_workspace(win);
  const Scene *scene = workspace->sequencer_scene;
  const Editing *ed = seq::editing_get(scene);
  const bToolRef *tref = area->runtime.tool;

  int wmcursor = WM_CURSOR_DEFAULT;

  if (tref == nullptr || scene == nullptr || ed == nullptr) {
    WM_cursor_set(win, wmcursor);
    return;
  }

  rcti scrub_rect = region->winrct;
  scrub_rect.ymin = scrub_rect.ymax - UI_TIME_SCRUB_MARGIN_Y;
  if (BLI_rcti_isect_pt_v(&scrub_rect, win->eventstate->xy)) {
    WM_cursor_set(win, wmcursor);
    return;
  }

  const View2D *v2d = &region->v2d;
  if (UI_view2d_mouse_in_scrollers(region, v2d, win->eventstate->xy)) {
    WM_cursor_set(win, wmcursor);
    return;
  }

  float mouse_co_region[2] = {float(win->eventstate->xy[0] - region->winrct.xmin),
                              float(win->eventstate->xy[1] - region->winrct.ymin)};
  float mouse_co_view[2];
  UI_view2d_region_to_view(
      &region->v2d, mouse_co_region[0], mouse_co_region[1], &mouse_co_view[0], &mouse_co_view[1]);

  if (STREQ(tref->idname, "builtin.blade") || STREQ(tref->idname, "builtin.slip")) {
    int mval[2] = {int(mouse_co_region[0]), int(mouse_co_region[1])};
    Strip *strip = strip_under_mouse_get(scene, v2d, mval);
    if (strip != nullptr) {
      ListBase *channels = seq::channels_displayed_get(ed);
      const bool locked = seq::transform_is_locked(channels, strip);
      const int frame = round_fl_to_int(mouse_co_view[0]);
      /* We cannot split the first and last frame, so blade cursor should not appear then. */
      if (STREQ(tref->idname, "builtin.blade") &&
          frame != seq::time_left_handle_frame_get(scene, strip) &&
          frame != seq::time_right_handle_frame_get(scene, strip))
      {
        wmcursor = locked ? WM_CURSOR_STOP : WM_CURSOR_BLADE;
      }
      else if (STREQ(tref->idname, "builtin.slip")) {
        wmcursor = (locked || seq::transform_single_image_check(strip)) ? WM_CURSOR_STOP :
                                                                          WM_CURSOR_SLIP;
      }
    }
    WM_cursor_set(win, wmcursor);
    return;
  }

  if (ed == nullptr) {
    WM_cursor_set(win, wmcursor);
    return;
  }

  StripSelection selection = pick_strip_and_handle(scene, &region->v2d, mouse_co_view);

  if (selection.strip1 == nullptr) {
    WM_cursor_set(win, wmcursor);
    return;
  }

  if (is_mouse_over_retiming_key(scene, selection.strip1, &region->v2d, area, mouse_co_region)) {
    WM_cursor_set(win, wmcursor);
    return;
  }

  if (!can_select_handle(scene, selection.strip1, v2d)) {
    WM_cursor_set(win, wmcursor);
    return;
  }

  if (selection.strip1 != nullptr && selection.strip2 != nullptr) {
    wmcursor = WM_CURSOR_BOTH_HANDLES;
  }
  else if (selection.handle == STRIP_HANDLE_LEFT) {
    wmcursor = WM_CURSOR_LEFT_HANDLE;
  }
  else if (selection.handle == STRIP_HANDLE_RIGHT) {
    wmcursor = WM_CURSOR_RIGHT_HANDLE;
  }

  WM_cursor_set(win, wmcursor);
}

/* *********************** header region ************************ */
/* Add handlers, stuff you only do once or on area/region changes. */
static void sequencer_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void sequencer_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

static void sequencer_footer_region_listener(const wmRegionListenerParams *params)
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

/* *********************** toolbar region ************************ */
/* Add handlers, stuff you only do once or on area/region changes. */
static void sequencer_tools_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Video Sequence Editor", SPACE_SEQ, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);
}

static void sequencer_tools_region_draw(const bContext *C, ARegion *region)
{
  ScrArea *area = CTX_wm_area(C);
  wm::OpCallContext op_context = wm::OpCallContext::InvokeRegionWin;

  LISTBASE_FOREACH (ARegion *, ar, &area->regionbase) {
    if (ar->regiontype == RGN_TYPE_PREVIEW && region->regiontype == RGN_TYPE_TOOLS) {
      op_context = wm::OpCallContext::InvokeRegionPreview;
      break;
    }
  }

  if (region->regiontype == RGN_TYPE_CHANNELS) {
    op_context = wm::OpCallContext::InvokeRegionChannels;
  }

  ED_region_panels_ex(C, region, op_context, nullptr);
}
/* *********************** preview region ************************ */

static bool sequencer_preview_region_poll(const RegionPollParams *params)
{
  const SpaceSeq *sseq = (SpaceSeq *)params->area->spacedata.first;
  return ELEM(sseq->view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW);
}

static void sequencer_preview_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_CUSTOM, region->winx, region->winy);

#if 0
  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Mask Editing", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);
#endif

  /* Own keymap. */
  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Preview", SPACE_SEQ, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Video Sequence Editor", SPACE_SEQ, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  /* Do this instead of adding V2D and frames `ED_KEYMAP_*` flags to `art->keymapflag`, since text
   * editing conflicts with several of their keymap items (e.g. arrow keys when editing text or
   * advancing frames). This seems to be the best way to define the proper order of evaluation. */
  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "View2D", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  keymap = WM_keymap_ensure(wm->runtime->defaultconf, "Frames", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  ListBase *lb = WM_dropboxmap_find("Sequencer", SPACE_SEQ, RGN_TYPE_PREVIEW);
  WM_event_add_dropbox_handler(&region->runtime->handlers, lb);
}

static void sequencer_preview_region_layout(const bContext *C, ARegion *region)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);

  if (sseq->flag & SEQ_ZOOM_TO_FIT) {
    View2D *v2d = &region->v2d;
    v2d->cur = v2d->tot;
  }
}

static void sequencer_preview_region_view2d_changed(const bContext *C, ARegion * /*region*/)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  sseq->flag &= ~SEQ_ZOOM_TO_FIT;
}

static void sequencer_preview_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  WM_gizmomap_tag_refresh(region->runtime->gizmo_map);

  /* Context changes. */
  switch (wmn->category) {
    case NC_OBJECT: /* To handle changes in 3D viewport. */
      switch (wmn->data) {
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_BONE_COLLECTION:
        case ND_TRANSFORM:
        case ND_POSE:
        case ND_DRAW:
        case ND_MODIFIER:
        case ND_SHADERFX:
        case ND_CONSTRAINT:
        case ND_KEYS:
        case ND_PARTICLE:
        case ND_POINTCACHE:
        case ND_LOD:
        case ND_DRAW_ANIMVIZ:
          ED_region_tag_redraw(region);
          break;
      }
      switch (wmn->action) {
        case NA_ADDED:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_GEOM: /* To handle changes in 3D viewport. */
      switch (wmn->data) {
        case ND_DATA:
        case ND_VERTEX_GROUP:
          ED_region_tag_redraw(region);
          break;
      }
      switch (wmn->action) {
        case NA_EDITED:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_MATERIAL: /* To handle changes in 3D viewport. */
      switch (wmn->data) {
        case ND_SHADING:
        case ND_NODES:
        case ND_SHADING_DRAW:
        case ND_SHADING_LINKS:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_NODE: /* To handle changes in 3D viewport. */
      ED_region_tag_redraw(region);
      break;
    case NC_WORLD: /* To handle changes in 3D viewport. */
      switch (wmn->data) {
        case ND_WORLD_DRAW:
        case ND_WORLD:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_LAMP: /* To handle changes in 3D viewport. */
      switch (wmn->data) {
        case ND_LIGHTING:
        case ND_LIGHTING_DRAW:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_LIGHTPROBE: /* To handle changes in 3D viewport. */
    case NC_IMAGE:
    case NC_TEXTURE:
      ED_region_tag_redraw(region);
      break;
    case NC_MOVIECLIP: /* To handle changes in 3D viewport. */
      if (wmn->data == ND_DISPLAY || wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      break;

    case NC_GPENCIL:
      if (ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        /* To handle changes in 3D viewport. */
        case ND_LAYER_CONTENT:
        case ND_LAYER:
        case ND_TRANSFORM:
        case ND_OB_VISIBLE:
        /* VSE related. */
        case ND_FRAME:
        case ND_MARKERS:
        case ND_SEQUENCER:
        case ND_RENDER_OPTIONS:
        case ND_DRAW_RENDER_VIEWPORT:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_ANIMATION:
      switch (wmn->data) {
        /* To handle changes in 3D viewport. */
        case ND_NLA_ACTCHANGE:
        case ND_NLA:
        case ND_ANIMCHAN:
        /* VSE related. */
        case ND_KEYFRAME:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_SEQUENCER) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_ID:
      switch (wmn->data) {
        case NA_RENAME:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_MASK:
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

/* *********************** buttons region ************************ */

/* Add handlers, stuff you only do once or on area/region changes. */
static void sequencer_buttons_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Video Sequence Editor", SPACE_SEQ, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);

  ED_region_panels_init(wm, region);
}

static void sequencer_buttons_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static void sequencer_buttons_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* Context changes. */
  switch (wmn->category) {
    case NC_GPENCIL:
      if (ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
        case ND_SEQUENCER:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_SEQUENCER) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

static void sequencer_id_remap(ScrArea * /*area*/,
                               SpaceLink *slink,
                               const bke::id::IDRemapper &mappings)
{
  SpaceSeq *sseq = (SpaceSeq *)slink;
  mappings.apply(reinterpret_cast<ID **>(&sseq->gpd), ID_REMAP_APPLY_DEFAULT);
}

static void sequencer_foreach_id(SpaceLink *space_link, LibraryForeachIDData *data)
{
  SpaceSeq *sseq = reinterpret_cast<SpaceSeq *>(space_link);
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, sseq->gpd, IDWALK_CB_USER | IDWALK_CB_DIRECT_WEAK_LINK);
}

/* ************************************* */

static bool sequencer_channel_region_poll(const RegionPollParams *params)
{
  const SpaceSeq *sseq = (SpaceSeq *)params->area->spacedata.first;
  return ELEM(sseq->view, SEQ_VIEW_SEQUENCE);
}

/* add handlers, stuff you only do once or on area/region changes */
static void sequencer_channel_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  region->alignment = RGN_ALIGN_LEFT;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_LIST, region->winx, region->winy);

  keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Sequencer Channels", SPACE_SEQ, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler_v2d_mask(&region->runtime->handlers, keymap);
}

static void sequencer_channel_region_draw(const bContext *C, ARegion *region)
{
  draw_channels(C, region);
}

static void sequencer_space_blend_read_data(BlendDataReader * /*reader*/, SpaceLink *sl)
{
  SpaceSeq *sseq = (SpaceSeq *)sl;
  sseq->runtime = MEM_new<SpaceSeq_Runtime>(__func__);

  /* grease pencil data is not a direct data and can't be linked from direct_link*
   * functions, it should be linked from lib_link* functions instead
   *
   * otherwise it'll lead to lost grease data on open because it'll likely be
   * read from file after all other users of grease pencil and newdataadr would
   * simple return nullptr here (sergey)
   */
#if 0
  if (sseq->gpd) {
    sseq->gpd = newdataadr(fd, sseq->gpd);
    BKE_gpencil_blend_read_data(fd, sseq->gpd);
  }
#endif
}

static void sequencer_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  BLO_write_struct(writer, SpaceSeq, sl);
}

void ED_spacetype_sequencer()
{
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_SEQ;
  STRNCPY_UTF8(st->name, "Sequencer");

  st->create = sequencer_create;
  st->free = sequencer_free;
  st->init = sequencer_init;
  st->duplicate = sequencer_duplicate;
  st->operatortypes = sequencer_operatortypes;
  st->keymap = sequencer_keymap;
  st->context = sequencer_context;
  st->gizmos = sequencer_gizmos;
  st->dropboxes = sequencer_dropboxes;
  st->refresh = sequencer_refresh;
  st->listener = sequencer_listener;
  st->id_remap = sequencer_id_remap;
  st->foreach_id = sequencer_foreach_id;
  st->blend_read_data = sequencer_space_blend_read_data;
  st->blend_read_after_liblink = nullptr;
  st->blend_write = sequencer_space_blend_write;

  /* Create regions: */
  /* Main window. */
  art = MEM_callocN<ARegionType>("spacetype sequencer region");
  art->regionid = RGN_TYPE_WINDOW;
  art->poll = sequencer_main_region_poll;
  art->init = sequencer_main_region_init;
  art->draw = sequencer_main_region_draw;
  art->draw_overlay = sequencer_main_region_draw_overlay;
  art->layout = sequencer_main_region_layout;
  art->on_view2d_changed = sequencer_main_region_view2d_changed;
  art->listener = sequencer_main_region_listener;
  art->message_subscribe = sequencer_main_region_message_subscribe;
  art->keymapflag = ED_KEYMAP_TOOL | ED_KEYMAP_GIZMO | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES |
                    ED_KEYMAP_ANIMATION;
  art->cursor = sequencer_main_cursor;
  art->event_cursor = true;
  art->clip_gizmo_events_by_ui = true;
  BLI_addhead(&st->regiontypes, art);

  /* Preview. */
  art = MEM_callocN<ARegionType>("spacetype sequencer region");
  art->regionid = RGN_TYPE_PREVIEW;
  art->poll = sequencer_preview_region_poll;
  art->init = sequencer_preview_region_init;
  art->layout = sequencer_preview_region_layout;
  art->on_view2d_changed = sequencer_preview_region_view2d_changed;
  art->draw = sequencer_preview_region_draw;
  art->listener = sequencer_preview_region_listener;
  art->keymapflag = ED_KEYMAP_TOOL | ED_KEYMAP_GIZMO | ED_KEYMAP_GPENCIL;
  BLI_addhead(&st->regiontypes, art);

  /* List-view/buttons. */
  art = MEM_callocN<ARegionType>("spacetype sequencer region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH * 1.3f;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->message_subscribe = ED_area_do_mgs_subscribe_for_tool_ui;
  art->listener = sequencer_buttons_region_listener;
  art->init = sequencer_buttons_region_init;
  art->snap_size = ED_region_generic_panel_region_snap_size;
  art->draw = sequencer_buttons_region_draw;
  BLI_addhead(&st->regiontypes, art);

  sequencer_buttons_register(art);
  /* Toolbar. */
  art = MEM_callocN<ARegionType>("spacetype sequencer tools region");
  art->regionid = RGN_TYPE_TOOLS;
  art->prefsizex = int(UI_TOOLBAR_WIDTH);
  art->prefsizey = 50; /* XXX */
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->message_subscribe = ED_region_generic_tools_region_message_subscribe;
  art->snap_size = ED_region_generic_tools_region_snap_size;
  art->init = sequencer_tools_region_init;
  art->draw = sequencer_tools_region_draw;
  art->listener = sequencer_main_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* Channels. */
  art = MEM_callocN<ARegionType>("spacetype sequencer channels");
  art->regionid = RGN_TYPE_CHANNELS;
  art->prefsizex = UI_COMPACT_PANEL_WIDTH;
  art->keymapflag = ED_KEYMAP_UI;
  art->poll = sequencer_channel_region_poll;
  art->init = sequencer_channel_region_init;
  art->draw = sequencer_channel_region_draw;
  art->listener = sequencer_main_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* Tool header. */
  art = MEM_callocN<ARegionType>("spacetype sequencer tool header region");
  art->regionid = RGN_TYPE_TOOL_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
  art->listener = sequencer_main_region_listener;
  art->init = sequencer_header_region_init;
  art->draw = sequencer_header_region_draw;
  art->message_subscribe = ED_area_do_mgs_subscribe_for_tool_header;
  BLI_addhead(&st->regiontypes, art);

  /* Header. */
  art = MEM_callocN<ARegionType>("spacetype sequencer region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;

  art->init = sequencer_header_region_init;
  art->draw = sequencer_header_region_draw;
  art->listener = sequencer_main_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* Footer. */
  art = MEM_callocN<ARegionType>("spacetype sequencer region");
  art->regionid = RGN_TYPE_FOOTER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FOOTER;

  art->init = sequencer_header_region_init;
  art->draw = sequencer_header_region_draw;
  art->listener = sequencer_footer_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* HUD. */
  art = ED_area_type_hud(st->spaceid);
  BLI_addhead(&st->regiontypes, art);

  WM_menutype_add(MEM_dupallocN<MenuType>(__func__, add_catalog_assets_menu_type()));
  WM_menutype_add(MEM_dupallocN<MenuType>(__func__, add_unassigned_assets_menu_type()));
  WM_menutype_add(MEM_dupallocN<MenuType>(__func__, add_scene_menu_type()));

  BKE_spacetype_register(std::move(st));

  /* Set the sequencer callback when not in background mode. */
  if (G.background == 0) {
    seq::view3d_fn = reinterpret_cast<seq::DrawViewFn>(ED_view3d_draw_offscreen_imbuf_simple);
  }
}

}  // namespace blender::ed::vse
