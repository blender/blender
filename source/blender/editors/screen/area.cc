/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscr
 */

#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_linklist_stack.h"
#include "BLI_rand.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "ED_asset_shelf.hh"
#include "ED_buttons.hh"
#include "ED_screen.hh"
#include "ED_screen_types.hh"
#include "ED_space_api.hh"
#include "ED_time_scrub_ui.hh"

#include "GPU_framebuffer.hh"
#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "BLF_api.hh"

#include "IMB_metadata.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "screen_intern.hh"

enum RegionEmbossSide {
  REGION_EMBOSS_LEFT = (1 << 0),
  REGION_EMBOSS_TOP = (1 << 1),
  REGION_EMBOSS_BOTTOM = (1 << 2),
  REGION_EMBOSS_RIGHT = (1 << 3),
  REGION_EMBOSS_ALL = REGION_EMBOSS_LEFT | REGION_EMBOSS_TOP | REGION_EMBOSS_RIGHT |
                      REGION_EMBOSS_BOTTOM,
};

/* general area and region code */

static void region_draw_emboss(const ARegion *region, const rcti *scirct, int sides)
{
  /* translate scissor rect to region space */
  rcti rect{};
  rect.xmin = scirct->xmin - region->winrct.xmin;
  rect.xmax = scirct->xmax - region->winrct.xmin;
  rect.ymin = scirct->ymin - region->winrct.ymin;
  rect.ymax = scirct->ymax - region->winrct.ymin;

  /* Set transparent line. */
  GPU_blend(GPU_BLEND_ALPHA);

  float color[4] = {0.0f, 0.0f, 0.0f, 0.25f};
  UI_GetThemeColor3fv(TH_EDITOR_OUTLINE, color);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4fv(color);

  immBeginAtMost(GPU_PRIM_LINES, 8);

  /* right */
  if (sides & REGION_EMBOSS_RIGHT) {
    immVertex2f(pos, rect.xmax, rect.ymax);
    immVertex2f(pos, rect.xmax, rect.ymin);
  }

  /* bottom */
  if (sides & REGION_EMBOSS_BOTTOM) {
    immVertex2f(pos, rect.xmax, rect.ymin);
    immVertex2f(pos, rect.xmin, rect.ymin);
  }

  /* left */
  if (sides & REGION_EMBOSS_LEFT) {
    immVertex2f(pos, rect.xmin, rect.ymin);
    immVertex2f(pos, rect.xmin, rect.ymax);
  }

  /* top */
  if (sides & REGION_EMBOSS_TOP) {
    immVertex2f(pos, rect.xmin, rect.ymax);
    immVertex2f(pos, rect.xmax, rect.ymax);
  }

  immEnd();
  immUnbindProgram();

  GPU_blend(GPU_BLEND_NONE);
}

void ED_region_pixelspace(const ARegion *region)
{
  wmOrtho2_region_pixelspace(region);
  GPU_matrix_identity_set();
}

void ED_region_do_listen(wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *notifier = params->notifier;

  /* generic notes first */
  switch (notifier->category) {
    case NC_WM:
      if (notifier->data == ND_FILEREAD) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_WINDOW:
      ED_region_tag_redraw(region);
      break;
  }

  if (region->type && region->type->listener) {
    region->type->listener(params);
  }

  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    UI_block_listen(block, params);
  }

  LISTBASE_FOREACH (uiList *, list, &region->ui_lists) {
    if (list->type && list->type->listener) {
      list->type->listener(list, params);
    }
  }
}

void ED_area_do_listen(wmSpaceTypeListenerParams *params)
{
  /* no generic notes? */
  if (params->area->type && params->area->type->listener) {
    params->area->type->listener(params);
  }
}

void ED_area_do_refresh(bContext *C, ScrArea *area)
{
  /* no generic notes? */
  if (area->type && area->type->refresh) {
    area->type->refresh(C, area);
  }
  area->do_refresh = false;
}

/**
 * \brief Corner widget use for quitting full-screen.
 */
static void area_draw_azone_fullscreen(short /*x1*/, short /*y1*/, short x2, short y2, float alpha)
{
  UI_icon_draw_ex(x2 - U.widget_unit,
                  y2 - U.widget_unit,
                  ICON_FULLSCREEN_EXIT,
                  UI_INV_SCALE_FAC,
                  min_ff(alpha, 0.75f),
                  0.0f,
                  nullptr,
                  false,
                  UI_NO_ICON_OVERLAY_TEXT);
}

/**
 * \brief Corner widgets use for dragging and splitting the view.
 */
static void area_draw_azone(short /*x1*/, short /*y1*/, short /*x2*/, short /*y2*/)
{
  /* No drawing needed since all corners are action zone, and visually distinguishable. */
}

/**
 * \brief Edge widgets to show hidden panels such as the toolbar and headers.
 */
static void draw_azone_arrow(float x1, float y1, float x2, float y2, AZEdge edge)
{
  const float size = 0.2f * U.widget_unit;
  const float l = 1.0f;  /* arrow length */
  const float s = 0.25f; /* arrow thickness */
  const float hl = l / 2.0f;
  const float points[6][2] = {
      {0, -hl}, {l, hl}, {l - s, hl + s}, {0, s + s - hl}, {s - l, hl + s}, {-l, hl}};
  const float center[2] = {(x1 + x2) / 2, (y1 + y2) / 2};

  int axis;
  int sign;
  switch (edge) {
    case AE_BOTTOM_TO_TOPLEFT:
      axis = 0;
      sign = 1;
      break;
    case AE_TOP_TO_BOTTOMRIGHT:
      axis = 0;
      sign = -1;
      break;
    case AE_LEFT_TO_TOPRIGHT:
      axis = 1;
      sign = 1;
      break;
    case AE_RIGHT_TO_TOPLEFT:
      axis = 1;
      sign = -1;
      break;
    default:
      BLI_assert(0);
      return;
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  GPU_blend(GPU_BLEND_ALPHA);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4f(0.8f, 0.8f, 0.8f, 0.4f);

  immBegin(GPU_PRIM_TRI_FAN, 6);
  for (int i = 0; i < 6; i++) {
    if (axis == 0) {
      immVertex2f(pos, center[0] + points[i][0] * size, center[1] + points[i][1] * sign * size);
    }
    else {
      immVertex2f(pos, center[0] + points[i][1] * sign * size, center[1] + points[i][0] * size);
    }
  }
  immEnd();

  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
}

static void region_draw_azone_tab_arrow(ScrArea *area, ARegion *region, AZone *az)
{
  GPU_blend(GPU_BLEND_ALPHA);

  /* add code to draw region hidden as 'too small' */
  switch (az->edge) {
    case AE_TOP_TO_BOTTOMRIGHT:
      UI_draw_roundbox_corner_set(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT);
      break;
    case AE_BOTTOM_TO_TOPLEFT:
      UI_draw_roundbox_corner_set(UI_CNR_BOTTOM_RIGHT | UI_CNR_BOTTOM_LEFT);
      break;
    case AE_LEFT_TO_TOPRIGHT:
      UI_draw_roundbox_corner_set(UI_CNR_TOP_LEFT | UI_CNR_BOTTOM_LEFT);
      break;
    case AE_RIGHT_TO_TOPLEFT:
      UI_draw_roundbox_corner_set(UI_CNR_TOP_RIGHT | UI_CNR_BOTTOM_RIGHT);
      break;
  }

  /* Workaround for different color spaces between normal areas and the ones using GPUViewports. */
  float alpha = WM_region_use_viewport(area, region) ? 0.6f : 0.4f;
  const float color[4] = {0.05f, 0.05f, 0.05f, alpha};
  rctf rect{};
  rect.xmin = float(az->x1);
  rect.xmax = float(az->x2);
  rect.ymin = float(az->y1);
  rect.ymax = float(az->y2);
  UI_draw_roundbox_aa(&rect, true, 4.0f, color);

  draw_azone_arrow(float(az->x1), float(az->y1), float(az->x2), float(az->y2), az->edge);
}

static void area_azone_tag_update(ScrArea *area)
{
  area->flag |= AREA_FLAG_ACTIONZONES_UPDATE;
}

static void region_draw_azones(ScrArea *area, ARegion *region)
{
  if (!area) {
    return;
  }

  GPU_line_width(1.0f);
  GPU_blend(GPU_BLEND_ALPHA);

  GPU_matrix_push();
  GPU_matrix_translate_2f(-region->winrct.xmin, -region->winrct.ymin);

  LISTBASE_FOREACH (AZone *, az, &area->actionzones) {
    /* test if action zone is over this region */
    rcti azrct;
    BLI_rcti_init(&azrct, az->x1, az->x2, az->y1, az->y2);

    if (BLI_rcti_isect(&region->drawrct, &azrct, nullptr)) {
      if (az->type == AZONE_AREA) {
        area_draw_azone(az->x1, az->y1, az->x2, az->y2);
      }
      else if (az->type == AZONE_REGION) {
        if (az->region && !(az->region->flag & RGN_FLAG_POLL_FAILED)) {
          /* only display tab or icons when the region is hidden */
          if (az->region->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL)) {
            region_draw_azone_tab_arrow(area, region, az);
          }
        }
      }
      else if (az->type == AZONE_FULLSCREEN) {
        if (az->alpha > 0.0f) {
          area_draw_azone_fullscreen(az->x1, az->y1, az->x2, az->y2, az->alpha);
        }
      }
    }
    if (!IS_EQF(az->alpha, 0.0f) && ELEM(az->type, AZONE_FULLSCREEN, AZONE_REGION_SCROLL)) {
      area_azone_tag_update(area);
    }
  }

  GPU_matrix_pop();

  GPU_blend(GPU_BLEND_NONE);
}

static void region_draw_status_text(ScrArea * /*area*/, ARegion *region)
{
  float header_color[4];
  UI_GetThemeColor4fv(TH_HEADER_ACTIVE, header_color);

  /* Clear the region from the buffer. */
  GPU_clear_color(0.0f, 0.0f, 0.0f, 0.0f);

  /* Fill with header color. */
  if (header_color[3] > 0.0f) {
    const rctf rect = {0.0f, float(region->winx), 0.0f, float(region->winy)};
    UI_draw_roundbox_4fv(&rect, true, 0.0f, header_color);
  }

  const int fontid = BLF_set_default();
  const float x = 12.0f * UI_SCALE_FAC;
  const float y = 0.4f * UI_UNIT_Y;
  GPU_blend(GPU_BLEND_ALPHA);

  if (header_color[3] < 0.3f) {
    /* Draw a background behind the text for extra contrast. */
    const float width = BLF_width(fontid, region->headerstr, BLF_DRAW_STR_DUMMY_MAX);
    const float pad = 5.0f * UI_SCALE_FAC;
    const float x1 = x - pad;
    const float x2 = x + width + pad;
    const float y1 = 3.0f * UI_SCALE_FAC;
    const float y2 = region->winy - (4.0f * UI_SCALE_FAC);
    float color[4] = {0.0f, 0.0f, 0.0f, 0.3f};
    UI_GetThemeColor3fv(TH_BACK, color);
    UI_draw_roundbox_corner_set(UI_CNR_ALL);
    const rctf rect = {x1, x2, y1, y2};
    UI_draw_roundbox_4fv(&rect, true, 4.0f * UI_SCALE_FAC, color);
  }

  UI_FontThemeColor(fontid, TH_TEXT);
  BLF_position(fontid, x, y, 0.0f);
  BLF_draw(fontid, region->headerstr, BLF_DRAW_STR_DUMMY_MAX);
}

void ED_region_do_msg_notify_tag_redraw(
    /* Follow wmMsgNotifyFn spec */
    bContext * /*C*/,
    wmMsgSubscribeKey * /*msg_key*/,
    wmMsgSubscribeValue *msg_val)
{
  ARegion *region = static_cast<ARegion *>(msg_val->owner);
  ED_region_tag_redraw(region);

  /* This avoids _many_ situations where header/properties control display settings.
   * the common case is space properties in the header */
  if (ELEM(region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER, RGN_TYPE_UI)) {
    while (region && region->prev) {
      region = region->prev;
    }
    for (; region; region = region->next) {
      if (ELEM(region->regiontype, RGN_TYPE_WINDOW, RGN_TYPE_CHANNELS)) {
        ED_region_tag_redraw(region);
      }
    }
  }
}

void ED_area_do_msg_notify_tag_refresh(
    /* Follow wmMsgNotifyFn spec */
    bContext * /*C*/,
    wmMsgSubscribeKey * /*msg_key*/,
    wmMsgSubscribeValue *msg_val)
{
  ScrArea *area = static_cast<ScrArea *>(msg_val->user_data);
  ED_area_tag_refresh(area);
}

void ED_area_do_mgs_subscribe_for_tool_header(const wmRegionMessageSubscribeParams *params)
{
  wmMsgBus *mbus = params->message_bus;
  WorkSpace *workspace = params->workspace;
  ARegion *region = params->region;

  BLI_assert(region->regiontype == RGN_TYPE_TOOL_HEADER);
  wmMsgSubscribeValue msg_sub_value_region_tag_redraw{};
  msg_sub_value_region_tag_redraw.owner = region;
  msg_sub_value_region_tag_redraw.user_data = region;
  msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;
  WM_msg_subscribe_rna_prop(
      mbus, &workspace->id, workspace, WorkSpace, tools, &msg_sub_value_region_tag_redraw);
}

void ED_area_do_mgs_subscribe_for_tool_ui(const wmRegionMessageSubscribeParams *params)
{
  wmMsgBus *mbus = params->message_bus;
  WorkSpace *workspace = params->workspace;
  ARegion *region = params->region;

  BLI_assert(region->regiontype == RGN_TYPE_UI);
  const char *panel_category_tool = "Tool";
  const char *category = UI_panel_category_active_get(region, false);

  bool update_region = false;
  if (category && STREQ(category, panel_category_tool)) {
    update_region = true;
  }
  else {
    /* Check if a tool category panel is pinned and visible in another category. */
    LISTBASE_FOREACH (Panel *, panel, &region->panels) {
      if (UI_panel_is_active(panel) && panel->flag & PNL_PIN &&
          STREQ(panel->type->category, panel_category_tool))
      {
        update_region = true;
        break;
      }
    }
  }

  if (update_region) {
    wmMsgSubscribeValue msg_sub_value_region_tag_redraw{};
    msg_sub_value_region_tag_redraw.owner = region;
    msg_sub_value_region_tag_redraw.user_data = region;
    msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;
    WM_msg_subscribe_rna_prop(
        mbus, &workspace->id, workspace, WorkSpace, tools, &msg_sub_value_region_tag_redraw);
  }
}

/**
 * Although there's no general support for minimizing areas, the status-bar can
 * be snapped to be only a few pixels high. A few pixels rather than 0 so it
 * can be un-minimized again. We consider it pseudo-minimized and don't draw
 * it then.
 */
static bool area_is_pseudo_minimized(const ScrArea *area)
{
  return (area->winx < 3) || (area->winy < 3);
}

void ED_region_do_layout(bContext *C, ARegion *region)
{
  /* This is optional, only needed for dynamically sized regions. */
  ScrArea *area = CTX_wm_area(C);
  ARegionType *at = region->type;

  if (!at->layout) {
    return;
  }

  if (at->do_lock || (area && area_is_pseudo_minimized(area))) {
    return;
  }

  region->do_draw |= RGN_DRAWING;

  UI_SetTheme(area ? area->spacetype : 0, at->regionid);
  at->layout(C, region);

  /* Clear temporary update flag. */
  region->flag &= ~RGN_FLAG_SEARCH_FILTER_UPDATE;
}

void ED_region_do_draw(bContext *C, ARegion *region)
{
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  ARegionType *at = region->type;

  /* see BKE_spacedata_draw_locks() */
  if (at->do_lock) {
    return;
  }

  region->do_draw |= RGN_DRAWING;

  /* Set viewport, scissor, ortho and region->drawrct. */
  wmPartialViewport(&region->drawrct, &region->winrct, &region->drawrct);

  wmOrtho2_region_pixelspace(region);

  UI_SetTheme(area ? area->spacetype : 0, at->regionid);

  if (area && area_is_pseudo_minimized(area)) {
    UI_ThemeClearColor(TH_EDITOR_OUTLINE);
    return;
  }
  /* optional header info instead? */
  if (region->headerstr) {
    region_draw_status_text(area, region);
  }
  else if (at->draw) {
    at->draw(C, region);
  }

  /* XXX test: add convention to end regions always in pixel space,
   * for drawing of borders/gestures etc */
  ED_region_pixelspace(region);

  /* Remove sRGB override by rebinding the framebuffer. */
  GPUFrameBuffer *fb = GPU_framebuffer_active_get();
  GPU_framebuffer_bind(fb);

  ED_region_draw_cb_draw(C, region, REGION_DRAW_POST_PIXEL);

  region_draw_azones(area, region);

  /* for debugging unneeded area redraws and partial redraw */
  if (G.debug_value == 888) {
    GPU_blend(GPU_BLEND_ALPHA);
    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor4f(BLI_thread_frand(0), BLI_thread_frand(0), BLI_thread_frand(0), 0.1f);
    immRectf(pos,
             region->drawrct.xmin - region->winrct.xmin,
             region->drawrct.ymin - region->winrct.ymin,
             region->drawrct.xmax - region->winrct.xmin,
             region->drawrct.ymax - region->winrct.ymin);
    immUnbindProgram();
    GPU_blend(GPU_BLEND_NONE);
  }

  memset(&region->drawrct, 0, sizeof(region->drawrct));

  UI_blocklist_free_inactive(C, region);

  if (area) {
    const bScreen *screen = WM_window_get_active_screen(win);

    /* Only region emboss for top-bar */
    if ((screen->state != SCREENFULL) && ED_area_is_global(area)) {
      region_draw_emboss(region, &region->winrct, (REGION_EMBOSS_LEFT | REGION_EMBOSS_RIGHT));
    }
    else if ((region->regiontype == RGN_TYPE_WINDOW) && (region->alignment == RGN_ALIGN_QSPLIT)) {

      /* draw separating lines between the quad views */

      float color[4] = {0.0f, 0.0f, 0.0f, 0.8f};
      UI_GetThemeColor3fv(TH_EDITOR_OUTLINE, color);
      GPUVertFormat *format = immVertexFormat();
      uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformColor4fv(color);
      GPU_line_width(1.0f);
      imm_draw_box_wire_2d(pos,
                           0,
                           0,
                           region->winrct.xmax - region->winrct.xmin + 1,
                           region->winrct.ymax - region->winrct.ymin + 1);
      immUnbindProgram();
    }
  }

  /* We may want to detach message-subscriptions from drawing. */
  {
    WorkSpace *workspace = CTX_wm_workspace(C);
    wmWindowManager *wm = CTX_wm_manager(C);
    bScreen *screen = WM_window_get_active_screen(win);
    Scene *scene = CTX_data_scene(C);
    wmMsgBus *mbus = wm->message_bus;
    WM_msgbus_clear_by_owner(mbus, region);

    /* Cheat, always subscribe to this space type properties.
     *
     * This covers most cases and avoids copy-paste similar code for each space type.
     */
    if (ELEM(region->regiontype, RGN_TYPE_WINDOW, RGN_TYPE_CHANNELS, RGN_TYPE_UI, RGN_TYPE_TOOLS))
    {
      SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);

      PointerRNA ptr = RNA_pointer_create(&screen->id, &RNA_Space, sl);

      /* All properties for this space type. */
      wmMsgSubscribeValue msg_sub_value_region_tag_redraw{};
      msg_sub_value_region_tag_redraw.owner = region;
      msg_sub_value_region_tag_redraw.user_data = region;
      msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;
      WM_msg_subscribe_rna(mbus, &ptr, nullptr, &msg_sub_value_region_tag_redraw, __func__);
    }

    wmRegionMessageSubscribeParams message_subscribe_params{};
    message_subscribe_params.context = C;
    message_subscribe_params.message_bus = mbus;
    message_subscribe_params.workspace = workspace;
    message_subscribe_params.scene = scene;
    message_subscribe_params.screen = screen;
    message_subscribe_params.area = area;
    message_subscribe_params.region = region;
    ED_region_message_subscribe(&message_subscribe_params);
  }
}

/* **********************************
 * maybe silly, but let's try for now
 * to keep these tags protected
 * ********************************** */

void ED_region_tag_redraw(ARegion *region)
{
  /* don't tag redraw while drawing, it shouldn't happen normally
   * but python scripts can cause this to happen indirectly */
  if (region && !(region->do_draw & RGN_DRAWING)) {
    /* zero region means full region redraw */
    region->do_draw &= ~(RGN_DRAW_PARTIAL | RGN_DRAW_NO_REBUILD | RGN_DRAW_EDITOR_OVERLAYS);
    region->do_draw |= RGN_DRAW;
    memset(&region->drawrct, 0, sizeof(region->drawrct));
  }
}

void ED_region_tag_redraw_cursor(ARegion *region)
{
  if (region) {
    region->do_draw_paintcursor = RGN_DRAW;
  }
}

void ED_region_tag_redraw_no_rebuild(ARegion *region)
{
  if (region && !(region->do_draw & (RGN_DRAWING | RGN_DRAW))) {
    region->do_draw &= ~(RGN_DRAW_PARTIAL | RGN_DRAW_EDITOR_OVERLAYS);
    region->do_draw |= RGN_DRAW_NO_REBUILD;
    memset(&region->drawrct, 0, sizeof(region->drawrct));
  }
}

void ED_region_tag_refresh_ui(ARegion *region)
{
  if (region) {
    region->do_draw |= RGN_REFRESH_UI;
  }
}

void ED_region_tag_redraw_editor_overlays(ARegion *region)
{
  if (region && !(region->do_draw & (RGN_DRAWING | RGN_DRAW))) {
    if (region->do_draw & RGN_DRAW_PARTIAL) {
      ED_region_tag_redraw(region);
    }
    else {
      region->do_draw |= RGN_DRAW_EDITOR_OVERLAYS;
    }
  }
}

void ED_region_tag_redraw_partial(ARegion *region, const rcti *rct, bool rebuild)
{
  if (region && !(region->do_draw & RGN_DRAWING)) {
    if (region->do_draw & RGN_DRAW_PARTIAL) {
      /* Partial redraw already set, expand region. */
      BLI_rcti_union(&region->drawrct, rct);
      if (rebuild) {
        region->do_draw &= ~RGN_DRAW_NO_REBUILD;
      }
    }
    else if (region->do_draw & (RGN_DRAW | RGN_DRAW_NO_REBUILD)) {
      /* Full redraw already requested. */
      if (rebuild) {
        region->do_draw &= ~RGN_DRAW_NO_REBUILD;
      }
    }
    else {
      /* No redraw set yet, set partial region. */
      region->drawrct = *rct;
      region->do_draw |= RGN_DRAW_PARTIAL;
      if (!rebuild) {
        region->do_draw |= RGN_DRAW_NO_REBUILD;
      }
    }
  }
}

void ED_area_tag_redraw(ScrArea *area)
{
  if (area) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      ED_region_tag_redraw(region);
    }
  }
}

void ED_area_tag_redraw_no_rebuild(ScrArea *area)
{
  if (area) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      ED_region_tag_redraw_no_rebuild(region);
    }
  }
}

void ED_area_tag_redraw_regiontype(ScrArea *area, int regiontype)
{
  if (area) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (region->regiontype == regiontype) {
        ED_region_tag_redraw(region);
      }
    }
  }
}

void ED_area_tag_refresh(ScrArea *area)
{
  if (area) {
    area->do_refresh = true;
  }
}

void ED_area_tag_region_size_update(ScrArea *area, ARegion *changed_region)
{
  if (!area || (area->flag & AREA_FLAG_REGION_SIZE_UPDATE)) {
    return;
  }

  area->flag |= AREA_FLAG_REGION_SIZE_UPDATE;

  /* Floating regions don't affect other regions, so the following can be skipped. */
  if (changed_region->alignment == RGN_ALIGN_FLOAT) {
    return;
  }

  /* Tag the following regions for redraw, since the size change of this region may affect the
   * available space for them. */
  for (ARegion *following_region = changed_region->next; following_region;
       following_region = following_region->next)
  {
    /* Overlapping and non-overlapping regions don't affect each others space. So layout changes
     * of one don't require redrawing the other. */
    if (changed_region->overlap != following_region->overlap) {
      continue;
    }
    /* Floating regions don't affect space of other regions. */
    if (following_region->alignment == RGN_ALIGN_FLOAT) {
      continue;
    }
    ED_region_tag_redraw(following_region);
  }
}

/* *************************************************************** */

const char *ED_area_region_search_filter_get(const ScrArea *area, const ARegion *region)
{
  /* Only the properties editor has a search string for now. */
  if (area->spacetype == SPACE_PROPERTIES) {
    SpaceProperties *sbuts = static_cast<SpaceProperties *>(area->spacedata.first);
    if (region->regiontype == RGN_TYPE_WINDOW) {
      return ED_buttons_search_string_get(sbuts);
    }
  }

  return nullptr;
}

void ED_region_search_filter_update(const ScrArea *area, ARegion *region)
{
  region->flag |= RGN_FLAG_SEARCH_FILTER_UPDATE;

  const char *search_filter = ED_area_region_search_filter_get(area, region);
  SET_FLAG_FROM_TEST(region->flag,
                     region->regiontype == RGN_TYPE_WINDOW && search_filter[0] != '\0',
                     RGN_FLAG_SEARCH_FILTER_ACTIVE);
}

/* *************************************************************** */

void ED_area_status_text(ScrArea *area, const char *str)
{
  /* happens when running transform operators in background mode */
  if (area == nullptr) {
    return;
  }

  ARegion *ar = nullptr;

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_HEADER && region->visible) {
      ar = region;
    }
    else if (region->regiontype == RGN_TYPE_TOOL_HEADER && region->visible) {
      /* Prefer tool header when we also have a header. */
      ar = region;
      break;
    }
  }

  if (ar) {
    if (str) {
      if (ar->headerstr == nullptr) {
        ar->headerstr = static_cast<char *>(MEM_mallocN(UI_MAX_DRAW_STR, "headerprint"));
      }
      BLI_strncpy(ar->headerstr, str, UI_MAX_DRAW_STR);
      BLI_str_rstrip(ar->headerstr);
    }
    else {
      MEM_SAFE_FREE(ar->headerstr);
    }
    ED_region_tag_redraw(ar);
  }
}

void ED_workspace_status_text(bContext *C, const char *str)
{
  wmWindow *win = CTX_wm_window(C);
  WorkSpace *workspace = CTX_wm_workspace(C);

  /* Can be nullptr when running operators in background mode. */
  if (workspace == nullptr) {
    return;
  }

  if (str) {
    if (workspace->status_text == nullptr) {
      workspace->status_text = static_cast<char *>(MEM_mallocN(UI_MAX_DRAW_STR, "headerprint"));
    }
    BLI_strncpy(workspace->status_text, str, UI_MAX_DRAW_STR);
  }
  else {
    MEM_SAFE_FREE(workspace->status_text);
  }

  /* Redraw status bar. */
  LISTBASE_FOREACH (ScrArea *, area, &win->global_areas.areabase) {
    if (area->spacetype == SPACE_STATUSBAR) {
      ED_area_tag_redraw(area);
      break;
    }
  }
}

/* ************************************************************ */

static void area_azone_init(wmWindow *win, const bScreen *screen, ScrArea *area)
{
  /* reinitialize entirely, regions and full-screen add azones too */
  BLI_freelistN(&area->actionzones);

  if (screen->state != SCREENNORMAL) {
    return;
  }

  if (U.app_flag & USER_APP_LOCK_CORNER_SPLIT) {
    return;
  }

  if (ED_area_is_global(area)) {
    return;
  }

  if (screen->temp) {
    return;
  }

  const float coords[4][4] = {
      /* Bottom-left. */
      {area->totrct.xmin - U.pixelsize,
       area->totrct.ymin - U.pixelsize,
       area->totrct.xmin + AZONESPOTW,
       area->totrct.ymin + AZONESPOTH},
      /* Bottom-right. */
      {area->totrct.xmax - AZONESPOTW,
       area->totrct.ymin - U.pixelsize,
       area->totrct.xmax + U.pixelsize,
       area->totrct.ymin + AZONESPOTH},
      /* Top-left. */
      {area->totrct.xmin - U.pixelsize,
       area->totrct.ymax - AZONESPOTH,
       area->totrct.xmin + AZONESPOTW,
       area->totrct.ymax + U.pixelsize},
      /* Top-right. */
      {area->totrct.xmax - AZONESPOTW,
       area->totrct.ymax - AZONESPOTH,
       area->totrct.xmax + U.pixelsize,
       area->totrct.ymax + U.pixelsize},
  };

  for (int i = 0; i < 4; i++) {
    /* can't click on bottom corners on OS X, already used for resizing */
#ifdef __APPLE__
    if (!WM_window_is_fullscreen(win) &&
        ((coords[i][0] == 0 && coords[i][1] == 0) ||
         (coords[i][0] == WM_window_pixels_x(win) && coords[i][1] == 0)))
    {
      continue;
    }
#else
    (void)win;
#endif

    /* set area action zones */
    AZone *az = (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
    BLI_addtail(&(area->actionzones), az);
    az->type = AZONE_AREA;
    az->x1 = coords[i][0];
    az->y1 = coords[i][1];
    az->x2 = coords[i][2];
    az->y2 = coords[i][3];
    BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
  }
}

static void fullscreen_azone_init(ScrArea *area, ARegion *region)
{
  if (ED_area_is_global(area) || (region->regiontype != RGN_TYPE_WINDOW)) {
    return;
  }

  AZone *az = (AZone *)MEM_callocN(sizeof(AZone), "fullscreen action zone");
  BLI_addtail(&(area->actionzones), az);
  az->type = AZONE_FULLSCREEN;
  az->region = region;
  az->alpha = 0.0f;

  if (U.uiflag2 & USER_REGION_OVERLAP) {
    const rcti *rect_visible = ED_region_visible_rect(region);
    az->x2 = region->winrct.xmin + rect_visible->xmax;
    az->y2 = region->winrct.ymin + rect_visible->ymax;
  }
  else {
    az->x2 = region->winrct.xmax;
    az->y2 = region->winrct.ymax;
  }
  az->x1 = az->x2 - AZONEFADEOUT;
  az->y1 = az->y2 - AZONEFADEOUT;

  BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
}

/**
 * Return true if the background color alpha is close to fully transparent. That is, a value of
 * less than 50 on a [0-255] scale (rather arbitrary threshold). Assumes the region uses #TH_BACK
 * for its background.
 */
static bool region_background_is_transparent(const ScrArea *area, const ARegion *region)
{
  /* Ensure the right theme is active, may not be the case on startup, for example. */
  bThemeState theme_state;
  UI_Theme_Store(&theme_state);
  UI_SetTheme(area->spacetype, region->regiontype);

  uchar back[4];
  UI_GetThemeColor4ubv(TH_BACK, back);

  UI_Theme_Restore(&theme_state);

  return back[3] < 50;
}

static void region_azone_edge(const ScrArea *area, AZone *az, const ARegion *region)
{
  const int azonepad_edge = (0.1f * U.widget_unit);

  /* If there is no visible region background, users typically expect the #AZone to be closer to
   * the content, so move it a bit. */
  const int overlap_padding =
      /* Header-like regions are usually thin and there's not much padding around them,
       * applying an offset would make the edge overlap buttons. */
      (!RGN_TYPE_IS_HEADER_ANY(region->regiontype) &&
       /* Is the region background transparent? */
       region->overlap && region_background_is_transparent(area, region)) ?
          /* Note that this is an arbitrary amount that matches nicely with numbers elsewhere. */
          int(0.4f * U.widget_unit) :
          0;

  switch (az->edge) {
    case AE_TOP_TO_BOTTOMRIGHT:
      az->x1 = region->winrct.xmin;
      az->y1 = region->winrct.ymax - azonepad_edge - overlap_padding;
      az->x2 = region->winrct.xmax;
      az->y2 = region->winrct.ymax + azonepad_edge - overlap_padding;
      break;
    case AE_BOTTOM_TO_TOPLEFT:
      az->x1 = region->winrct.xmin;
      az->y1 = region->winrct.ymin + azonepad_edge + overlap_padding;
      az->x2 = region->winrct.xmax;
      az->y2 = region->winrct.ymin - azonepad_edge + overlap_padding;
      break;
    case AE_LEFT_TO_TOPRIGHT:
      az->x1 = region->winrct.xmin - azonepad_edge + overlap_padding;
      az->y1 = region->winrct.ymin;
      az->x2 = region->winrct.xmin + azonepad_edge + overlap_padding;
      az->y2 = region->winrct.ymax;
      break;
    case AE_RIGHT_TO_TOPLEFT:
      az->x1 = region->winrct.xmax + azonepad_edge - overlap_padding;
      az->y1 = region->winrct.ymin;
      az->x2 = region->winrct.xmax - azonepad_edge - overlap_padding;
      az->y2 = region->winrct.ymax;
      break;
  }
  BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
}

/* region already made zero sized, in shape of edge */
static void region_azone_tab_plus(ScrArea *area, AZone *az, ARegion *region)
{
  float edge_offset = 1.0f;
  const float tab_size_x = 0.7f * U.widget_unit;
  const float tab_size_y = 0.4f * U.widget_unit;

  switch (az->edge) {
    case AE_TOP_TO_BOTTOMRIGHT: {
      int add = (region->winrct.ymax == area->totrct.ymin) ? 1 : 0;
      az->x1 = region->winrct.xmax - ((edge_offset + 1.0f) * tab_size_x);
      az->y1 = region->winrct.ymax - add;
      az->x2 = region->winrct.xmax - (edge_offset * tab_size_x);
      az->y2 = region->winrct.ymax - add + tab_size_y;
      break;
    }
    case AE_BOTTOM_TO_TOPLEFT:
      az->x1 = region->winrct.xmax - ((edge_offset + 1.0f) * tab_size_x);
      az->y1 = region->winrct.ymin - tab_size_y;
      az->x2 = region->winrct.xmax - (edge_offset * tab_size_x);
      az->y2 = region->winrct.ymin;
      break;
    case AE_LEFT_TO_TOPRIGHT:
      az->x1 = region->winrct.xmin - tab_size_y;
      az->y1 = region->winrct.ymax - ((edge_offset + 1.0f) * tab_size_x);
      az->x2 = region->winrct.xmin;
      az->y2 = region->winrct.ymax - (edge_offset * tab_size_x);
      break;
    case AE_RIGHT_TO_TOPLEFT:
      az->x1 = region->winrct.xmax;
      az->y1 = region->winrct.ymax - ((edge_offset + 1.0f) * tab_size_x);
      az->x2 = region->winrct.xmax + tab_size_y;
      az->y2 = region->winrct.ymax - (edge_offset * tab_size_x);
      break;
  }
  /* rect needed for mouse pointer test */
  BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
}

static bool region_azone_edge_poll(const ARegion *region, const bool is_fullscreen)
{
  if (region->flag & RGN_FLAG_POLL_FAILED) {
    return false;
  }

  /* Don't use edge if the region hides with previous region which is now hidden. See #116196. */
  if ((region->alignment & (RGN_SPLIT_PREV | RGN_ALIGN_HIDE_WITH_PREV) && region->prev) &&
      region->prev->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL))
  {
    return false;
  }

  const bool is_hidden = (region->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL));

  if (is_hidden && is_fullscreen) {
    return false;
  }
  if (!is_hidden && ELEM(region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
    return false;
  }

  if (is_hidden && (U.app_flag & USER_APP_HIDE_REGION_TOGGLE)) {
    return false;
  }

  if (!is_hidden && (U.app_flag & USER_APP_LOCK_EDGE_RESIZE)) {
    return false;
  }

  return true;
}

static void region_azone_edge_init(ScrArea *area,
                                   ARegion *region,
                                   AZEdge edge,
                                   const bool is_fullscreen)
{
  const bool is_hidden = (region->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL));

  if (!region_azone_edge_poll(region, is_fullscreen)) {
    return;
  }

  AZone *az = (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
  BLI_addtail(&(area->actionzones), az);
  az->type = AZONE_REGION;
  az->region = region;
  az->edge = edge;

  if (is_hidden) {
    region_azone_tab_plus(area, az, region);
  }
  else {
    region_azone_edge(area, az, region);
  }
}

static void region_azone_scrollbar_init(ScrArea *area,
                                        ARegion *region,
                                        AZScrollDirection direction)
{
  AZone *az = static_cast<AZone *>(MEM_callocN(sizeof(*az), __func__));

  BLI_addtail(&area->actionzones, az);
  az->type = AZONE_REGION_SCROLL;
  az->region = region;
  az->direction = direction;

  if (direction == AZ_SCROLL_VERT) {
    az->region->v2d.alpha_vert = 0;
  }
  else if (direction == AZ_SCROLL_HOR) {
    az->region->v2d.alpha_hor = 0;
  }

  /* No need to specify rect for scrollbar az. For intersection we'll test against the area around
   * the region's scroller instead, in `area_actionzone_get_rect`. */
}

static void region_azones_scrollbars_init(ScrArea *area, ARegion *region)
{
  const View2D *v2d = &region->v2d;

  if ((v2d->scroll & V2D_SCROLL_VERTICAL) && ((v2d->scroll & V2D_SCROLL_VERTICAL_HANDLES) == 0)) {
    region_azone_scrollbar_init(area, region, AZ_SCROLL_VERT);
  }
  if ((v2d->scroll & V2D_SCROLL_HORIZONTAL) &&
      ((v2d->scroll & V2D_SCROLL_HORIZONTAL_HANDLES) == 0))
  {
    region_azone_scrollbar_init(area, region, AZ_SCROLL_HOR);
  }
}

/* *************************************************************** */
static void region_azones_add_edge(ScrArea *area,
                                   ARegion *region,
                                   const int alignment,
                                   const bool is_fullscreen)
{

  /* edge code (t b l r) is along which area edge azone will be drawn */
  if (alignment == RGN_ALIGN_TOP) {
    region_azone_edge_init(area, region, AE_BOTTOM_TO_TOPLEFT, is_fullscreen);
  }
  else if (alignment == RGN_ALIGN_BOTTOM) {
    region_azone_edge_init(area, region, AE_TOP_TO_BOTTOMRIGHT, is_fullscreen);
  }
  else if (alignment == RGN_ALIGN_RIGHT) {
    region_azone_edge_init(area, region, AE_LEFT_TO_TOPRIGHT, is_fullscreen);
  }
  else if (alignment == RGN_ALIGN_LEFT) {
    region_azone_edge_init(area, region, AE_RIGHT_TO_TOPLEFT, is_fullscreen);
  }
}

static void region_azones_add(const bScreen *screen, ScrArea *area, ARegion *region)
{
  const bool is_fullscreen = screen->state == SCREENFULL;

  /* Only display tab or icons when the header region is hidden
   * (not the tool header - they overlap). */
  if (region->regiontype == RGN_TYPE_TOOL_HEADER) {
    return;
  }

  region_azones_add_edge(area, region, RGN_ALIGN_ENUM_FROM_MASK(region->alignment), is_fullscreen);

  /* For a split region also continue the azone edge from the next region if this region is aligned
   * with the next */
  if ((region->alignment & RGN_SPLIT_PREV) && region->prev) {
    region_azones_add_edge(
        area, region, RGN_ALIGN_ENUM_FROM_MASK(region->prev->alignment), is_fullscreen);
  }

  if (is_fullscreen) {
    fullscreen_azone_init(area, region);
  }

  region_azones_scrollbars_init(area, region);
}

/* dir is direction to check, not the splitting edge direction! */
static int rct_fits(const rcti *rect, const eScreenAxis dir_axis, int size)
{
  if (dir_axis == SCREEN_AXIS_H) {
    return BLI_rcti_size_x(rect) + 1 - size;
  }
  /* Vertical. */
  return BLI_rcti_size_y(rect) + 1 - size;
}

/* *************************************************************** */

/* region should be overlapping */
/* function checks if some overlapping region was defined before - on same place */
static void region_overlap_fix(ScrArea *area, ARegion *region)
{
  /* find overlapping previous region on same place */
  ARegion *region_iter;
  int align1 = 0;
  const int align = RGN_ALIGN_ENUM_FROM_MASK(region->alignment);
  for (region_iter = region->prev; region_iter; region_iter = region_iter->prev) {
    if (region_iter->flag & (RGN_FLAG_POLL_FAILED | RGN_FLAG_HIDDEN)) {
      continue;
    }
    if (!region_iter->overlap || (region_iter->alignment & RGN_SPLIT_PREV)) {
      continue;
    }

    const int align_iter = RGN_ALIGN_ENUM_FROM_MASK(region_iter->alignment);
    if (ELEM(align_iter, RGN_ALIGN_FLOAT)) {
      continue;
    }

    align1 = align_iter;
    if (BLI_rcti_isect(&region_iter->winrct, &region->winrct, nullptr)) {
      if (align1 != align) {
        /* Left overlapping right or vice-versa, forbid this! */
        region->flag |= RGN_FLAG_TOO_SMALL;
        return;
      }
      /* Else, we have our previous region on same side. */
      break;
    }
  }

  /* Guard against flags slipping through that would have to be masked out in usages below. */
  BLI_assert(align1 == RGN_ALIGN_ENUM_FROM_MASK(align1));

  /* translate or close */
  if (region_iter) {
    if (align1 == RGN_ALIGN_LEFT) {
      if (region->winrct.xmax + region_iter->winx > area->winx - U.widget_unit) {
        region->flag |= RGN_FLAG_TOO_SMALL;
        return;
      }
      BLI_rcti_translate(&region->winrct, region_iter->winx, 0);
    }
    else if (align1 == RGN_ALIGN_RIGHT) {
      if (region->winrct.xmin - region_iter->winx < U.widget_unit) {
        region->flag |= RGN_FLAG_TOO_SMALL;
        return;
      }
      BLI_rcti_translate(&region->winrct, -region_iter->winx, 0);
    }
  }

  /* At this point, 'region' is in its final position and still open.
   * Make a final check it does not overlap any previous 'other side' region. */
  for (region_iter = region->prev; region_iter; region_iter = region_iter->prev) {
    if (region_iter->flag & (RGN_FLAG_POLL_FAILED | RGN_FLAG_HIDDEN)) {
      continue;
    }
    if (!region_iter->overlap || (region_iter->alignment & RGN_SPLIT_PREV)) {
      continue;
    }

    const int align_iter = RGN_ALIGN_ENUM_FROM_MASK(region_iter->alignment);
    if (ELEM(align_iter, RGN_ALIGN_FLOAT)) {
      continue;
    }

    if ((align_iter != align) && BLI_rcti_isect(&region_iter->winrct, &region->winrct, nullptr)) {
      /* Left overlapping right or vice-versa, forbid this! */
      region->flag |= RGN_FLAG_TOO_SMALL;
      return;
    }
  }
}

bool ED_region_is_overlap(int spacetype, int regiontype)
{
  if (regiontype == RGN_TYPE_HUD) {
    return true;
  }
  if (U.uiflag2 & USER_REGION_OVERLAP) {
    if (spacetype == SPACE_NODE) {
      if (regiontype == RGN_TYPE_TOOLS) {
        return true;
      }
    }
    else if (spacetype == SPACE_VIEW3D) {
      if (regiontype == RGN_TYPE_HEADER) {
        /* Do not treat as overlapped if no transparency. */
        bTheme *theme = UI_GetTheme();
        return theme->space_view3d.header[3] != 255;
      }
      if (ELEM(regiontype,
               RGN_TYPE_TOOLS,
               RGN_TYPE_UI,
               RGN_TYPE_TOOL_PROPS,
               RGN_TYPE_FOOTER,
               RGN_TYPE_TOOL_HEADER,
               RGN_TYPE_ASSET_SHELF,
               RGN_TYPE_ASSET_SHELF_HEADER))
      {
        return true;
      }
    }
    else if (spacetype == SPACE_IMAGE) {
      if (ELEM(regiontype,
               RGN_TYPE_TOOLS,
               RGN_TYPE_UI,
               RGN_TYPE_TOOL_PROPS,
               RGN_TYPE_FOOTER,
               RGN_TYPE_TOOL_HEADER,
               RGN_TYPE_ASSET_SHELF,
               RGN_TYPE_ASSET_SHELF_HEADER))
      {
        return true;
      }
    }
  }

  return false;
}

static void region_rect_recursive(
    ScrArea *area, ARegion *region, rcti *remainder, rcti *overlap_remainder, int quad)
{
  using namespace blender::ed;
  rcti *remainder_prev = remainder;

  if (region == nullptr) {
    return;
  }

  int prev_winx = region->winx;
  int prev_winy = region->winy;

  /* no returns in function, winrct gets set in the end again */
  BLI_rcti_init(&region->winrct, 0, 0, 0, 0);

  /* for test; allow split of previously defined region */
  if (region->alignment & RGN_SPLIT_PREV) {
    if (region->prev) {
      remainder = &region->prev->winrct;
    }
  }

  int alignment = RGN_ALIGN_ENUM_FROM_MASK(region->alignment);

  /* set here, assuming userpref switching forces to call this again */
  region->overlap = ED_region_is_overlap(area->spacetype, region->regiontype);

  /* clear state flags first */
  region->flag &= ~(RGN_FLAG_TOO_SMALL | RGN_FLAG_SIZE_CLAMP_X | RGN_FLAG_SIZE_CLAMP_Y);
  /* user errors */
  if ((region->next == nullptr) && !ELEM(alignment, RGN_ALIGN_QSPLIT, RGN_ALIGN_FLOAT)) {
    alignment = RGN_ALIGN_NONE;
  }

  /* If both the #ARegion.sizex/y and the #ARegionType.prefsizex/y are 0, the region is tagged as
   * too small, even before the layout for dynamically sized regions is created.
   * #wm_draw_window_offscreen() allows the layout to be created despite the #RGN_FLAG_TOO_SMALL
   * flag being set. But there may still be regions that don't have a separate #ARegionType.layout
   * callback. For those, set a default #ARegionType.prefsizex/y so they can become visible. */
  if ((region->flag & RGN_FLAG_DYNAMIC_SIZE) && !(region->type->layout)) {
    if ((region->sizex == 0) && (region->type->prefsizex == 0)) {
      region->type->prefsizex = AREAMINX;
    }
    if ((region->sizey == 0) && (region->type->prefsizey == 0)) {
      region->type->prefsizey = HEADERY;
    }
  }

  /* `prefsizex/y`, taking into account DPI. */
  int prefsizex = UI_SCALE_FAC *
                  ((region->sizex > 1) ? region->sizex + 0.5f : region->type->prefsizex);
  int prefsizey;

  if (region->regiontype == RGN_TYPE_HEADER) {
    prefsizey = ED_area_headersize();
  }
  else if (region->regiontype == RGN_TYPE_TOOL_HEADER) {
    prefsizey = ED_area_headersize();
  }
  else if (region->regiontype == RGN_TYPE_FOOTER) {
    prefsizey = ED_area_footersize();
  }
  else if (region->regiontype == RGN_TYPE_ASSET_SHELF) {
    prefsizey = region->sizey > 1 ? (UI_SCALE_FAC * (region->sizey + 0.5f)) :
                                    asset::shelf::region_prefsizey();
  }
  else if (region->regiontype == RGN_TYPE_ASSET_SHELF_HEADER) {
    prefsizey = asset::shelf::header_region_size();
  }
  else if (ED_area_is_global(area)) {
    prefsizey = ED_region_global_size_y();
  }
  else {
    prefsizey = UI_SCALE_FAC *
                (region->sizey > 1 ? region->sizey + 0.5f : region->type->prefsizey);
  }

  if (region->flag & (RGN_FLAG_POLL_FAILED | RGN_FLAG_HIDDEN)) {
    /* hidden is user flag */
  }
  else if (alignment == RGN_ALIGN_FLOAT) {
    /**
     * \note Currently this window type is only used for #RGN_TYPE_HUD,
     * We expect the panel to resize itself to be larger.
     *
     * This aligns to the lower left of the area.
     */
    const int size_min[2] = {UI_UNIT_X, UI_UNIT_Y};
    rcti overlap_remainder_margin = *overlap_remainder;

    BLI_rcti_resize(&overlap_remainder_margin,
                    max_ii(0, BLI_rcti_size_x(overlap_remainder) - UI_UNIT_X / 2),
                    max_ii(0, BLI_rcti_size_y(overlap_remainder) - UI_UNIT_Y / 2));
    region->winrct.xmin = overlap_remainder_margin.xmin + region->runtime.offset_x;
    region->winrct.ymin = overlap_remainder_margin.ymin + region->runtime.offset_y;
    region->winrct.xmax = region->winrct.xmin + prefsizex - 1;
    region->winrct.ymax = region->winrct.ymin + prefsizey - 1;

    BLI_rcti_isect(&region->winrct, &overlap_remainder_margin, &region->winrct);

    if (BLI_rcti_size_x(&region->winrct) != prefsizex - 1) {
      region->flag |= RGN_FLAG_SIZE_CLAMP_X;
    }
    if (BLI_rcti_size_y(&region->winrct) != prefsizey - 1) {
      region->flag |= RGN_FLAG_SIZE_CLAMP_Y;
    }

    /* We need to use a test that won't have been previously clamped. */
    rcti winrct_test{};
    winrct_test.xmin = region->winrct.xmin;
    winrct_test.ymin = region->winrct.ymin;
    winrct_test.xmax = region->winrct.xmin + size_min[0];
    winrct_test.ymax = region->winrct.ymin + size_min[1];

    BLI_rcti_isect(&winrct_test, &overlap_remainder_margin, &winrct_test);
    if (BLI_rcti_size_x(&winrct_test) < size_min[0] || BLI_rcti_size_y(&winrct_test) < size_min[1])
    {
      region->flag |= RGN_FLAG_TOO_SMALL;
    }
  }
  else if (rct_fits(remainder, SCREEN_AXIS_V, 1) < 0 || rct_fits(remainder, SCREEN_AXIS_H, 1) < 0)
  {
    /* remainder is too small for any usage */
    region->flag |= RGN_FLAG_TOO_SMALL;
  }
  else if (alignment == RGN_ALIGN_NONE) {
    /* typically last region */
    region->winrct = *remainder;
    BLI_rcti_init(remainder, 0, 0, 0, 0);
  }
  else if (ELEM(alignment, RGN_ALIGN_TOP, RGN_ALIGN_BOTTOM)) {
    rcti *winrct = (region->overlap) ? overlap_remainder : remainder;

    if ((prefsizey == 0) || (rct_fits(winrct, SCREEN_AXIS_V, prefsizey) < 0)) {
      region->flag |= RGN_FLAG_TOO_SMALL;
    }
    else {
      int fac = rct_fits(winrct, SCREEN_AXIS_V, prefsizey);

      if (fac < 0) {
        prefsizey += fac;
      }

      region->winrct = *winrct;

      if (alignment == RGN_ALIGN_TOP) {
        region->winrct.ymin = region->winrct.ymax - prefsizey + 1;
        winrct->ymax = region->winrct.ymin - 1;
      }
      else {
        region->winrct.ymax = region->winrct.ymin + prefsizey - 1;
        winrct->ymin = region->winrct.ymax + 1;
      }
      BLI_rcti_sanitize(winrct);
    }
  }
  else if (ELEM(alignment, RGN_ALIGN_LEFT, RGN_ALIGN_RIGHT)) {
    rcti *winrct = (region->overlap) ? overlap_remainder : remainder;

    if ((prefsizex == 0) || (rct_fits(winrct, SCREEN_AXIS_H, prefsizex) < 0)) {
      region->flag |= RGN_FLAG_TOO_SMALL;
    }
    else {
      int fac = rct_fits(winrct, SCREEN_AXIS_H, prefsizex);

      if (fac < 0) {
        prefsizex += fac;
      }

      region->winrct = *winrct;

      if (alignment == RGN_ALIGN_RIGHT) {
        region->winrct.xmin = region->winrct.xmax - prefsizex + 1;
        winrct->xmax = region->winrct.xmin - 1;
      }
      else {
        region->winrct.xmax = region->winrct.xmin + prefsizex - 1;
        winrct->xmin = region->winrct.xmax + 1;
      }
      BLI_rcti_sanitize(winrct);
    }
  }
  else if (ELEM(alignment, RGN_ALIGN_VSPLIT, RGN_ALIGN_HSPLIT)) {
    /* Percentage subdiv. */
    region->winrct = *remainder;

    if (alignment == RGN_ALIGN_HSPLIT) {
      if (rct_fits(remainder, SCREEN_AXIS_H, prefsizex) > 4) {
        region->winrct.xmax = BLI_rcti_cent_x(remainder);
        remainder->xmin = region->winrct.xmax + 1;
      }
      else {
        BLI_rcti_init(remainder, 0, 0, 0, 0);
      }
    }
    else {
      if (rct_fits(remainder, SCREEN_AXIS_V, prefsizey) > 4) {
        region->winrct.ymax = BLI_rcti_cent_y(remainder);
        remainder->ymin = region->winrct.ymax + 1;
      }
      else {
        BLI_rcti_init(remainder, 0, 0, 0, 0);
      }
    }
  }
  else if (alignment == RGN_ALIGN_QSPLIT) {
    region->winrct = *remainder;

    /* test if there's still 4 regions left */
    if (quad == 0) {
      ARegion *region_test = region->next;
      int count = 1;

      while (region_test) {
        region_test->alignment = RGN_ALIGN_QSPLIT;
        region_test = region_test->next;
        count++;
      }

      if (count != 4) {
        /* let's stop adding regions */
        BLI_rcti_init(remainder, 0, 0, 0, 0);
        if (G.debug & G_DEBUG) {
          printf("region quadsplit failed\n");
        }
      }
      else {
        quad = 1;
      }
    }
    if (quad) {
      if (quad == 1) { /* left bottom */
        region->winrct.xmax = BLI_rcti_cent_x(remainder);
        region->winrct.ymax = BLI_rcti_cent_y(remainder);
      }
      else if (quad == 2) { /* left top */
        region->winrct.xmax = BLI_rcti_cent_x(remainder);
        region->winrct.ymin = BLI_rcti_cent_y(remainder) + 1;
      }
      else if (quad == 3) { /* right bottom */
        region->winrct.xmin = BLI_rcti_cent_x(remainder) + 1;
        region->winrct.ymax = BLI_rcti_cent_y(remainder);
      }
      else { /* right top */
        region->winrct.xmin = BLI_rcti_cent_x(remainder) + 1;
        region->winrct.ymin = BLI_rcti_cent_y(remainder) + 1;
        BLI_rcti_init(remainder, 0, 0, 0, 0);
      }

      /* Fix any negative dimensions. This can happen when a quad split 3d view gets too small.
       * (see #72200). */
      BLI_rcti_sanitize(&region->winrct);

      quad++;
    }
  }

  /* for speedup */
  region->winx = BLI_rcti_size_x(&region->winrct) + 1;
  region->winy = BLI_rcti_size_y(&region->winrct) + 1;

  /* If region opened normally, we store this for hide/reveal usage. */
  /* Prevent rounding errors for UI_SCALE_FAC multiply and divide. */
  if (region->winx > 1) {
    region->sizex = (region->winx + 0.5f) / UI_SCALE_FAC;
  }
  if (region->winy > 1) {
    region->sizey = (region->winy + 0.5f) / UI_SCALE_FAC;
  }

  /* exception for multiple overlapping regions on same spot */
  if (region->overlap && (alignment != RGN_ALIGN_FLOAT)) {
    region_overlap_fix(area, region);
  }

  /* Set `region->winrct` for action-zones. */
  if (region->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL)) {
    region->winrct = (region->overlap) ? *overlap_remainder : *remainder;

    switch (alignment) {
      case RGN_ALIGN_TOP:
        region->winrct.ymin = region->winrct.ymax;
        break;
      case RGN_ALIGN_BOTTOM:
        region->winrct.ymax = region->winrct.ymin;
        break;
      case RGN_ALIGN_RIGHT:
        region->winrct.xmin = region->winrct.xmax;
        break;
      case RGN_ALIGN_LEFT:
        region->winrct.xmax = region->winrct.xmin;
        break;
      default:
        /* prevent winrct to be valid */
        region->winrct.xmax = region->winrct.xmin;
        break;
    }

    /* Size on one axis is now 0, the other axis may still be invalid (negative) though. */
    BLI_rcti_sanitize(&region->winrct);
  }

  /* restore prev-split exception */
  if (region->alignment & RGN_SPLIT_PREV) {
    if (region->prev) {
      remainder = remainder_prev;
      region->prev->winx = BLI_rcti_size_x(&region->prev->winrct) + 1;
      region->prev->winy = BLI_rcti_size_y(&region->prev->winrct) + 1;
    }
  }

  /* After non-overlapping region, all following overlapping regions
   * fit within the remaining space again. */
  if (!region->overlap) {
    *overlap_remainder = *remainder;
  }

  BLI_assert(BLI_rcti_is_valid(&region->winrct));

  region_rect_recursive(area, region->next, remainder, overlap_remainder, quad);

  /* Tag for redraw if size changes. */
  if (region->winx != prev_winx || region->winy != prev_winy) {
    /* 3D View needs a full rebuild in case a progressive render runs. Rest can live with
     * no-rebuild (e.g. Outliner) */
    if (area->spacetype == SPACE_VIEW3D) {
      ED_region_tag_redraw(region);
    }
    else {
      ED_region_tag_redraw_no_rebuild(region);
    }
  }

  /* Clear, initialize on demand. */
  memset(&region->runtime.visible_rect, 0, sizeof(region->runtime.visible_rect));
}

static void area_calc_totrct(ScrArea *area, const rcti *window_rect)
{
  short px = short(U.pixelsize);

  area->totrct.xmin = area->v1->vec.x;
  area->totrct.xmax = area->v4->vec.x;
  area->totrct.ymin = area->v1->vec.y;
  area->totrct.ymax = area->v2->vec.y;

  /* scale down totrct by 1 pixel on all sides not matching window borders */
  if (area->totrct.xmin > window_rect->xmin) {
    area->totrct.xmin += px;
  }
  if (area->totrct.xmax < (window_rect->xmax - 1)) {
    area->totrct.xmax -= px;
  }
  if (area->totrct.ymin > window_rect->ymin) {
    area->totrct.ymin += px;
  }
  if (area->totrct.ymax < (window_rect->ymax - 1)) {
    area->totrct.ymax -= px;
  }
  /* Although the following asserts are correct they lead to a very unstable Blender.
   * And the asserts would fail even in 2.7x
   * (they were added in 2.8x as part of the top-bar commit).
   * For more details see #54864. */
#if 0
  BLI_assert(area->totrct.xmin >= 0);
  BLI_assert(area->totrct.xmax >= 0);
  BLI_assert(area->totrct.ymin >= 0);
  BLI_assert(area->totrct.ymax >= 0);
#endif

  /* for speedup */
  area->winx = BLI_rcti_size_x(&area->totrct) + 1;
  area->winy = BLI_rcti_size_y(&area->totrct) + 1;
}

/**
 * Update the `ARegion::visible` flag.
 */
static void region_evaulate_visibility(ARegion *region)
{
  bool hidden = (region->flag & (RGN_FLAG_POLL_FAILED | RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL)) !=
                0;

  if ((region->alignment & (RGN_SPLIT_PREV | RGN_ALIGN_HIDE_WITH_PREV)) && region->prev) {
    hidden = hidden || (region->prev->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL));
  }

  region->visible = !hidden;
}

static bool event_in_markers_region(const ARegion *region, const wmEvent *event)
{
  rcti rect = region->winrct;
  rect.ymax = rect.ymin + UI_MARKER_MARGIN_Y;
  return BLI_rcti_isect_pt_v(&rect, event->xy);
}

/**
 * \param region: Region, may be nullptr when adding handlers for \a area.
 */
static void ed_default_handlers(
    wmWindowManager *wm, ScrArea *area, ARegion *region, ListBase *handlers, int flag)
{
  BLI_assert(region ? (&region->handlers == handlers) : (&area->handlers == handlers));

  /* NOTE: add-handler checks if it already exists. */

  /* XXX: it would be good to have bound-box checks for some of these. */
  if (flag & ED_KEYMAP_UI) {
    wmKeyMap *keymap = WM_keymap_ensure(
        wm->defaultconf, "User Interface", SPACE_EMPTY, RGN_TYPE_WINDOW);
    WM_event_add_keymap_handler(handlers, keymap);

    ListBase *dropboxes = WM_dropboxmap_find("User Interface", SPACE_EMPTY, RGN_TYPE_WINDOW);
    WM_event_add_dropbox_handler(handlers, dropboxes);

    /* user interface widgets */
    UI_region_handlers_add(handlers);
  }
  if (flag & ED_KEYMAP_GIZMO) {
    BLI_assert(region && ELEM(region->type->regionid, RGN_TYPE_WINDOW, RGN_TYPE_PREVIEW));
    if (region) {
      /* Anything else is confusing, only allow this. */
      BLI_assert(&region->handlers == handlers);
      if (region->gizmo_map == nullptr) {
        wmGizmoMapType_Params params{};
        params.spaceid = area->spacetype;
        params.regionid = region->type->regionid;
        region->gizmo_map = WM_gizmomap_new_from_type(&params);
      }
      WM_gizmomap_add_handlers(region, region->gizmo_map);
    }
  }
  if (flag & ED_KEYMAP_VIEW2D) {
    /* 2d-viewport handling+manipulation */
    wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "View2D", SPACE_EMPTY, RGN_TYPE_WINDOW);
    WM_event_add_keymap_handler(handlers, keymap);
  }
  if (flag & ED_KEYMAP_ANIMATION) {
    wmKeyMap *keymap;

    /* time-markers */
    keymap = WM_keymap_ensure(wm->defaultconf, "Markers", SPACE_EMPTY, RGN_TYPE_WINDOW);
    WM_event_add_keymap_handler_poll(handlers, keymap, event_in_markers_region);

    /* time-scrub */
    keymap = WM_keymap_ensure(wm->defaultconf, "Time Scrub", SPACE_EMPTY, RGN_TYPE_WINDOW);
    WM_event_add_keymap_handler_poll(handlers, keymap, ED_time_scrub_event_in_region);

    /* frame changing and timeline operators (for time spaces) */
    keymap = WM_keymap_ensure(wm->defaultconf, "Animation", SPACE_EMPTY, RGN_TYPE_WINDOW);
    WM_event_add_keymap_handler(handlers, keymap);
  }
  if (flag & ED_KEYMAP_TOOL) {
    if (flag & ED_KEYMAP_GIZMO) {
      WM_event_add_keymap_handler_dynamic(
          &region->handlers, WM_event_get_keymap_from_toolsystem_with_gizmos, area);
    }
    else {
      WM_event_add_keymap_handler_dynamic(
          &region->handlers, WM_event_get_keymap_from_toolsystem, area);
    }
  }
  if (flag & ED_KEYMAP_FRAMES) {
    /* frame changing/jumping (for all spaces) */
    wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "Frames", SPACE_EMPTY, RGN_TYPE_WINDOW);
    WM_event_add_keymap_handler(handlers, keymap);
  }
  if (flag & ED_KEYMAP_HEADER) {
    /* standard keymap for headers regions */
    wmKeyMap *keymap = WM_keymap_ensure(
        wm->defaultconf, "Region Context Menu", SPACE_EMPTY, RGN_TYPE_WINDOW);
    WM_event_add_keymap_handler(handlers, keymap);
  }
  if (flag & ED_KEYMAP_FOOTER) {
    /* standard keymap for footer regions */
    wmKeyMap *keymap = WM_keymap_ensure(
        wm->defaultconf, "Region Context Menu", SPACE_EMPTY, RGN_TYPE_WINDOW);
    WM_event_add_keymap_handler(handlers, keymap);
  }
  if (flag & ED_KEYMAP_NAVBAR) {
    /* standard keymap for Navigation bar regions */
    wmKeyMap *keymap = WM_keymap_ensure(
        wm->defaultconf, "Region Context Menu", SPACE_EMPTY, RGN_TYPE_WINDOW);
    WM_event_add_keymap_handler(&region->handlers, keymap);
  }
  if (flag & ED_KEYMAP_ASSET_SHELF) {
    /* standard keymap for asset shelf regions */
    wmKeyMap *keymap = WM_keymap_ensure(
        wm->defaultconf, "Asset Shelf", SPACE_EMPTY, RGN_TYPE_WINDOW);
    WM_event_add_keymap_handler(&region->handlers, keymap);
  }

  /* Keep last because of LMB/RMB handling, see: #57527. */
  if (flag & ED_KEYMAP_GPENCIL) {
    /* grease pencil */
    /* NOTE: This is now 4 keymaps - One for basic functionality,
     *       and others for special stroke modes (edit, paint and sculpt).
     *
     *       For now, it's easier to just include all,
     *       since you hardly want one without the others.
     */
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Curve Edit Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Edit Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Paint Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf,
                                          "Grease Pencil Stroke Paint (Draw brush)",
                                          SPACE_EMPTY,
                                          RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Paint (Erase)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Paint (Fill)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Paint (Tint)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Sculpt Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Vertex Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Vertex (Draw)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Vertex (Blur)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Vertex (Average)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Vertex (Smear)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Vertex (Replace)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Sculpt (Smooth)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf,
                                          "Grease Pencil Stroke Sculpt (Thickness)",
                                          SPACE_EMPTY,
                                          RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Sculpt (Strength)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Sculpt (Grab)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Sculpt (Push)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Sculpt (Twist)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Sculpt (Pinch)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf,
                                          "Grease Pencil Stroke Sculpt (Randomize)",
                                          SPACE_EMPTY,
                                          RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Sculpt (Clone)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Weight Mode", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Weight (Draw)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Weight (Blur)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Weight (Average)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
    {
      wmKeyMap *keymap = WM_keymap_ensure(
          wm->defaultconf, "Grease Pencil Stroke Weight (Smear)", SPACE_EMPTY, RGN_TYPE_WINDOW);
      WM_event_add_keymap_handler(handlers, keymap);
    }
  }
}

void ED_area_update_region_sizes(wmWindowManager *wm, wmWindow *win, ScrArea *area)
{
  if (!(area->flag & AREA_FLAG_REGION_SIZE_UPDATE)) {
    return;
  }
  const bScreen *screen = WM_window_get_active_screen(win);

  rcti window_rect;
  WM_window_rect_calc(win, &window_rect);
  area_calc_totrct(area, &window_rect);

  /* region rect sizes */
  rcti rect = area->totrct;
  rcti overlap_rect = rect;
  region_rect_recursive(
      area, static_cast<ARegion *>(area->regionbase.first), &rect, &overlap_rect, 0);

  /* Dynamically sized regions may have changed region sizes, so we have to force azone update. */
  area_azone_init(win, screen, area);

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->flag & RGN_FLAG_POLL_FAILED) {
      continue;
    }
    region_evaulate_visibility(region);

    /* region size may have changed, init does necessary adjustments */
    if (region->type->init) {
      region->type->init(wm, region);
    }

    /* Some AZones use View2D data which is only updated in region init, so call that first! */
    region_azones_add(screen, area, region);
  }
  ED_area_azones_update(area, win->eventstate->xy);

  area->flag &= ~AREA_FLAG_REGION_SIZE_UPDATE;
}

bool ED_area_has_shared_border(ScrArea *a, ScrArea *b)
{
  return area_getorientation(a, b) != -1;
}

/**
 * Setup a known space type in the event a file with an unknown space-type is loaded.
 */
static void area_init_type_fallback(ScrArea *area, eSpace_Type space_type)
{
  BLI_assert(area->type == nullptr);
  area->spacetype = space_type;
  area->type = BKE_spacetype_from_id(area->spacetype);

  SpaceLink *sl = nullptr;
  LISTBASE_FOREACH (SpaceLink *, sl_iter, &area->spacedata) {
    if (sl_iter->spacetype == space_type) {
      sl = sl_iter;
      break;
    }
  }
  if (sl) {
    SpaceLink *sl_old = static_cast<SpaceLink *>(area->spacedata.first);
    if (LIKELY(sl != sl_old)) {
      BLI_remlink(&area->spacedata, sl);
      BLI_addhead(&area->spacedata, sl);

      /* swap regions */
      sl_old->regionbase = area->regionbase;
      area->regionbase = sl->regionbase;
      BLI_listbase_clear(&sl->regionbase);
    }
  }
  else {
    screen_area_spacelink_add(nullptr, area, space_type);
  }
}

void ED_area_init(wmWindowManager *wm, wmWindow *win, ScrArea *area)
{
  WorkSpace *workspace = WM_window_get_active_workspace(win);
  const bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);
  const Scene *scene = WM_window_get_active_scene(win);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);

  if (ED_area_is_global(area) && (area->global->flag & GLOBAL_AREA_IS_HIDDEN)) {
    return;
  }

  rcti window_rect;
  WM_window_rect_calc(win, &window_rect);

  /* Set type-definitions. */
  area->type = BKE_spacetype_from_id(area->spacetype);

  if (area->type == nullptr) {
    area_init_type_fallback(area, SPACE_VIEW3D);
    BLI_assert(area->type != nullptr);
  }

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    region->type = BKE_regiontype_from_id(area->type, region->regiontype);
    /* Invalid region types may be stored in files (e.g. for new files), but they should be handled
     * on file read already, see #BKE_screen_area_blend_read_lib(). */
    BLI_assert_msg(region->type != nullptr, "Region type not valid for this space type");
  }

  /* area sizes */
  area_calc_totrct(area, &window_rect);

  /* region rect sizes */
  rcti rect = area->totrct;
  rcti overlap_rect = rect;
  region_rect_recursive(
      area, static_cast<ARegion *>(area->regionbase.first), &rect, &overlap_rect, 0);
  area->flag &= ~AREA_FLAG_REGION_SIZE_UPDATE;

  /* default area handlers */
  ed_default_handlers(wm, area, nullptr, &area->handlers, area->type->keymapflag);
  /* checks spacedata, adds own handlers */
  if (area->type->init) {
    area->type->init(wm, area);
  }

  /* clear all azones, add the area triangle widgets */
  area_azone_init(win, screen, area);

  /* region windows, default and own handlers */
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    region_evaulate_visibility(region);

    if (region->visible) {
      /* default region handlers */
      ed_default_handlers(wm, area, region, &region->handlers, region->type->keymapflag);
      /* own handlers */
      if (region->type->init) {
        region->type->init(wm, region);
      }
    }
    else {
      /* prevent uiblocks to run */
      UI_blocklist_free(nullptr, region);
    }

    /* Some AZones use View2D data which is only updated in region init, so call that first! */
    region_azones_add(screen, area, region);
  }

  /* Avoid re-initializing tools while resizing the window. */
  if ((G.moving & G_TRANSFORM_WM) == 0) {
    if ((1 << area->spacetype) & WM_TOOLSYSTEM_SPACE_MASK) {
      WM_toolsystem_refresh_screen_area(workspace, scene, view_layer, area);
      area->flag |= AREA_FLAG_ACTIVE_TOOL_UPDATE;
    }
    else {
      area->runtime.tool = nullptr;
      area->runtime.is_tool_set = true;
    }
  }
}

static void area_offscreen_init(ScrArea *area)
{
  area->flag |= AREA_FLAG_OFFSCREEN;
  area->type = BKE_spacetype_from_id(area->spacetype);
  /* Off screen areas are only ever created at run-time,
   * so there is no reason for the type to be unknown. */
  BLI_assert(area->type != nullptr);

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    region->type = BKE_regiontype_from_id(area->type, region->regiontype);
  }
}

ScrArea *ED_area_offscreen_create(wmWindow *win, eSpace_Type space_type)
{
  ScrArea *area = static_cast<ScrArea *>(MEM_callocN(sizeof(ScrArea), __func__));
  area->spacetype = space_type;

  screen_area_spacelink_add(WM_window_get_active_scene(win), area, space_type);
  area_offscreen_init(area);

  return area;
}

static void area_offscreen_exit(wmWindowManager *wm, wmWindow *win, ScrArea *area)
{
  if (area->type && area->type->exit) {
    area->type->exit(wm, area);
  }

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->type && region->type->exit) {
      region->type->exit(wm, region);
    }

    WM_event_modal_handler_region_replace(win, region, nullptr);
    WM_draw_region_free(region);
    region->visible = false;

    MEM_SAFE_FREE(region->headerstr);

    if (region->regiontimer) {
      WM_event_timer_remove(wm, win, region->regiontimer);
      region->regiontimer = nullptr;
    }

    if (wm->message_bus) {
      WM_msgbus_clear_by_owner(wm->message_bus, region);
    }
  }

  WM_event_modal_handler_area_replace(win, area, nullptr);
}

void ED_area_offscreen_free(wmWindowManager *wm, wmWindow *win, ScrArea *area)
{
  area_offscreen_exit(wm, win, area);

  BKE_screen_area_free(area);
  MEM_freeN(area);
}

static void region_update_rect(ARegion *region)
{
  region->winx = BLI_rcti_size_x(&region->winrct) + 1;
  region->winy = BLI_rcti_size_y(&region->winrct) + 1;

  /* v2d mask is used to subtract scrollbars from a 2d view. Needs initialize here. */
  BLI_rcti_init(&region->v2d.mask, 0, region->winx - 1, 0, region->winy - 1);
}

void ED_region_update_rect(ARegion *region)
{
  region_update_rect(region);
}

void ED_region_floating_init(ARegion *region)
{
  BLI_assert(region->alignment == RGN_ALIGN_FLOAT);

  /* refresh can be called before window opened */
  region_evaulate_visibility(region);

  region_update_rect(region);
}

void ED_region_cursor_set(wmWindow *win, ScrArea *area, ARegion *region)
{
  if (region != nullptr) {
    if ((region->gizmo_map != nullptr) && WM_gizmomap_cursor_set(region->gizmo_map, win)) {
      return;
    }
    if (area && region->type && region->type->cursor) {
      region->type->cursor(win, area, region);
      return;
    }
  }

  if (WM_cursor_set_from_tool(win, area, region)) {
    return;
  }

  WM_cursor_set(win, WM_CURSOR_DEFAULT);
}

void ED_region_visibility_change_update_ex(
    bContext *C, ScrArea *area, ARegion *region, bool is_hidden, bool do_init)
{
  if (is_hidden) {
    WM_event_remove_handlers(C, &region->handlers);
    /* Needed to close any open pop-overs which would otherwise remain open,
     * crashing on attempting to refresh. See: #93410.
     *
     * When #ED_area_init frees buttons via #UI_blocklist_free a nullptr context
     * is passed, causing the free not to remove menus or their handlers. */
    UI_region_free_active_but_all(C, region);
  }

  if (do_init) {
    ED_area_init(CTX_wm_manager(C), CTX_wm_window(C), area);
    ED_area_tag_redraw(area);
  }
}

void ED_region_visibility_change_update(bContext *C, ScrArea *area, ARegion *region)
{
  const bool is_hidden = region->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_POLL_FAILED);
  const bool do_init = true;
  ED_region_visibility_change_update_ex(C, area, region, is_hidden, do_init);
}

void region_toggle_hidden(bContext *C, ARegion *region, const bool do_fade)
{
  ScrArea *area = CTX_wm_area(C);

  region->flag ^= RGN_FLAG_HIDDEN;

  if (do_fade && region->overlap) {
    /* starts a timer, and in end calls the stuff below itself (region_sblend_invoke()) */
    ED_region_visibility_change_update_animated(C, area, region);
  }
  else {
    ED_region_visibility_change_update(C, area, region);
  }
}

void ED_region_toggle_hidden(bContext *C, ARegion *region)
{
  region_toggle_hidden(C, region, true);
}

void ED_area_data_copy(ScrArea *area_dst, ScrArea *area_src, const bool do_free)
{
  const char spacetype = area_dst->spacetype;
  const short flag_copy = HEADER_NO_PULLDOWN;

  area_dst->spacetype = area_src->spacetype;
  area_dst->type = area_src->type;

  area_dst->flag = (area_dst->flag & ~flag_copy) | (area_src->flag & flag_copy);

  /* area */
  if (do_free) {
    BKE_spacedata_freelist(&area_dst->spacedata);
  }
  BKE_spacedata_copylist(&area_dst->spacedata, &area_src->spacedata);

  /* NOTE: SPACE_EMPTY is possible on new screens. */

  /* regions */
  if (do_free) {
    SpaceType *st = BKE_spacetype_from_id(spacetype);
    LISTBASE_FOREACH (ARegion *, region, &area_dst->regionbase) {
      BKE_area_region_free(st, region);
    }
    BLI_freelistN(&area_dst->regionbase);
  }
  SpaceType *st = BKE_spacetype_from_id(area_src->spacetype);
  LISTBASE_FOREACH (ARegion *, region, &area_src->regionbase) {
    ARegion *newar = BKE_area_region_copy(st, region);
    BLI_addtail(&area_dst->regionbase, newar);
  }
}

void ED_area_data_swap(ScrArea *area_dst, ScrArea *area_src)
{
  std::swap(area_dst->spacetype, area_src->spacetype);
  std::swap(area_dst->type, area_src->type);

  std::swap(area_dst->spacedata, area_src->spacedata);
  std::swap(area_dst->regionbase, area_src->regionbase);
}

/* -------------------------------------------------------------------- */
/** \name Region Alignment Syncing for Space Switching
 * \{ */

/**
 * Store the alignment & other info per region type
 * (use as a region-type aligned array).
 *
 * \note Currently this is only done for headers,
 * we might want to do this with the tool-bar in the future too.
 */
struct RegionTypeAlignInfo {
  struct {
    /**
     * Values match #ARegion.alignment without flags (see #RGN_ALIGN_ENUM_FROM_MASK).
     * store all so we can sync alignment without adding extra checks.
     */
    short alignment;
    /**
     * Needed for detecting which header displays the space-type switcher.
     */
    bool hidden;
  } by_type[RGN_TYPE_NUM];
};

static void region_align_info_from_area(ScrArea *area, RegionTypeAlignInfo *r_align_info)
{
  for (int index = 0; index < RGN_TYPE_NUM; index++) {
    r_align_info->by_type[index].alignment = -1;
    /* Default to true, when it doesn't exist - it's effectively hidden. */
    r_align_info->by_type[index].hidden = true;
  }

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->flag & RGN_FLAG_POLL_FAILED) {
      continue;
    }

    const int index = region->regiontype;
    if (uint(index) < RGN_TYPE_NUM) {
      r_align_info->by_type[index].alignment = RGN_ALIGN_ENUM_FROM_MASK(region->alignment);
      r_align_info->by_type[index].hidden = (region->flag & RGN_FLAG_HIDDEN) != 0;
    }
  }
}

/**
 * Keeping alignment between headers keep the space-type selector button in the same place.
 * This is complicated by the editor-type selector being placed on the header
 * closest to the screen edge which changes based on hidden state.
 *
 * The tool-header is used when visible, otherwise the header is used.
 */
static short region_alignment_from_header_and_tool_header_state(
    const RegionTypeAlignInfo *region_align_info, const short fallback)
{
  const short header_alignment = region_align_info->by_type[RGN_TYPE_HEADER].alignment;
  const short tool_header_alignment = region_align_info->by_type[RGN_TYPE_TOOL_HEADER].alignment;

  const bool header_hidden = region_align_info->by_type[RGN_TYPE_HEADER].hidden;
  const bool tool_header_hidden = region_align_info->by_type[RGN_TYPE_TOOL_HEADER].hidden;

  if ((tool_header_alignment != -1) &&
      /* If tool-header is hidden, use header alignment. */
      ((tool_header_hidden == false) ||
       /* Don't prioritize the tool-header if both are hidden (behave as if both are visible).
        * Without this, switching to a space with headers hidden will flip the alignment
        * upon switching to a space with visible headers. */
       (header_hidden && tool_header_hidden)))
  {
    return tool_header_alignment;
  }
  if (header_alignment != -1) {
    return header_alignment;
  }
  return fallback;
}

/**
 * Notes on header alignment syncing.
 *
 * This is as involved as it is because:
 *
 * - There are currently 3 kinds of headers.
 * - All headers can independently visible & flipped to another side
 *   (except for the tool-header that depends on the header visibility).
 * - We don't want the space-switching button to flip when switching spaces.
 *   From the user perspective it feels like a bug to move the button you click on
 *   to the opposite side of the area.
 * - The space-switcher may be on either the header or the tool-header
 *   depending on the tool-header visibility.
 *
 * How this works:
 *
 * - When headers match on both spaces, we copy the alignment
 *   from the previous regions to the next regions when syncing.
 * - Otherwise detect the _primary_ header (the one that shows the space type)
 *   and use this to set alignment for the headers in the destination area.
 * - Header & tool-header/footer may be on opposite sides, this is preserved when syncing.
 */
static void region_align_info_to_area_for_headers(const RegionTypeAlignInfo *region_align_info_src,
                                                  const RegionTypeAlignInfo *region_align_info_dst,
                                                  ARegion *region_by_type[RGN_TYPE_NUM])
{
  /* Abbreviate access. */
  const short header_alignment_src = region_align_info_src->by_type[RGN_TYPE_HEADER].alignment;
  const short tool_header_alignment_src =
      region_align_info_src->by_type[RGN_TYPE_TOOL_HEADER].alignment;

  const bool tool_header_hidden_src = region_align_info_src->by_type[RGN_TYPE_TOOL_HEADER].hidden;

  const short primary_header_alignment_src = region_alignment_from_header_and_tool_header_state(
      region_align_info_src, -1);

  /* Neither alignments are usable, don't sync. */
  if (primary_header_alignment_src == -1) {
    return;
  }

  const short header_alignment_dst = region_align_info_dst->by_type[RGN_TYPE_HEADER].alignment;
  const short tool_header_alignment_dst =
      region_align_info_dst->by_type[RGN_TYPE_TOOL_HEADER].alignment;
  const short footer_alignment_dst = region_align_info_dst->by_type[RGN_TYPE_FOOTER].alignment;

  const bool tool_header_hidden_dst = region_align_info_dst->by_type[RGN_TYPE_TOOL_HEADER].hidden;

  /* New synchronized alignments to set (or ignore when left as -1). */
  short header_alignment_sync = -1;
  short tool_header_alignment_sync = -1;
  short footer_alignment_sync = -1;

  /* Both source/destination areas have same region configurations regarding headers.
   * Simply copy the values. */
  if (((header_alignment_src != -1) == (header_alignment_dst != -1)) &&
      ((tool_header_alignment_src != -1) == (tool_header_alignment_dst != -1)) &&
      (tool_header_hidden_src == tool_header_hidden_dst))
  {
    if (header_alignment_dst != -1) {
      header_alignment_sync = header_alignment_src;
    }
    if (tool_header_alignment_dst != -1) {
      tool_header_alignment_sync = tool_header_alignment_src;
    }
  }
  else {
    /* Not an exact match, check the space selector isn't moving. */
    const short primary_header_alignment_dst = region_alignment_from_header_and_tool_header_state(
        region_align_info_dst, -1);

    if (primary_header_alignment_src != primary_header_alignment_dst) {
      if ((header_alignment_dst != -1) && (tool_header_alignment_dst != -1)) {
        if (header_alignment_dst == tool_header_alignment_dst) {
          /* Apply to both. */
          tool_header_alignment_sync = primary_header_alignment_src;
          header_alignment_sync = primary_header_alignment_src;
        }
        else {
          /* Keep on opposite sides. */
          tool_header_alignment_sync = primary_header_alignment_src;
          header_alignment_sync = (tool_header_alignment_sync == RGN_ALIGN_BOTTOM) ?
                                      RGN_ALIGN_TOP :
                                      RGN_ALIGN_BOTTOM;
        }
      }
      else {
        /* Apply what we can to regions that exist. */
        if (header_alignment_dst != -1) {
          header_alignment_sync = primary_header_alignment_src;
        }
        if (tool_header_alignment_dst != -1) {
          tool_header_alignment_sync = primary_header_alignment_src;
        }
      }
    }
  }

  if (footer_alignment_dst != -1) {
    if ((header_alignment_dst != -1) && (header_alignment_dst == footer_alignment_dst)) {
      /* Apply to both. */
      footer_alignment_sync = primary_header_alignment_src;
    }
    else {
      /* Keep on opposite sides. */
      footer_alignment_sync = (primary_header_alignment_src == RGN_ALIGN_BOTTOM) ?
                                  RGN_ALIGN_TOP :
                                  RGN_ALIGN_BOTTOM;
    }
  }

  /* Finally apply synchronized flags. */
  if (header_alignment_sync != -1) {
    ARegion *region = region_by_type[RGN_TYPE_HEADER];
    if (region != nullptr) {
      region->alignment = RGN_ALIGN_ENUM_FROM_MASK(header_alignment_sync) |
                          RGN_ALIGN_FLAG_FROM_MASK(region->alignment);
    }
  }

  if (tool_header_alignment_sync != -1) {
    ARegion *region = region_by_type[RGN_TYPE_TOOL_HEADER];
    if (region != nullptr) {
      region->alignment = RGN_ALIGN_ENUM_FROM_MASK(tool_header_alignment_sync) |
                          RGN_ALIGN_FLAG_FROM_MASK(region->alignment);
    }
  }

  if (footer_alignment_sync != -1) {
    ARegion *region = region_by_type[RGN_TYPE_FOOTER];
    if (region != nullptr) {
      region->alignment = RGN_ALIGN_ENUM_FROM_MASK(footer_alignment_sync) |
                          RGN_ALIGN_FLAG_FROM_MASK(region->alignment);
    }
  }
}

static void region_align_info_to_area(
    ScrArea *area, const RegionTypeAlignInfo region_align_info_src[RGN_TYPE_NUM])
{
  ARegion *region_by_type[RGN_TYPE_NUM] = {nullptr};
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    const int index = region->regiontype;
    if (uint(index) < RGN_TYPE_NUM) {
      region_by_type[index] = region;
    }
  }

  RegionTypeAlignInfo region_align_info_dst;
  region_align_info_from_area(area, &region_align_info_dst);

  if ((region_by_type[RGN_TYPE_HEADER] != nullptr) ||
      (region_by_type[RGN_TYPE_TOOL_HEADER] != nullptr))
  {
    region_align_info_to_area_for_headers(
        region_align_info_src, &region_align_info_dst, region_by_type);
  }

  /* Note that we could support other region types. */
}

/** \} */

/* *********** Space switching code *********** */

void ED_area_swapspace(bContext *C, ScrArea *sa1, ScrArea *sa2)
{
  ScrArea *tmp = static_cast<ScrArea *>(MEM_callocN(sizeof(ScrArea), __func__));
  wmWindow *win = CTX_wm_window(C);

  ED_area_exit(C, sa1);
  ED_area_exit(C, sa2);

  ED_area_data_copy(tmp, sa1, false);
  ED_area_data_copy(sa1, sa2, true);
  ED_area_data_copy(sa2, tmp, true);
  ED_area_init(CTX_wm_manager(C), win, sa1);
  ED_area_init(CTX_wm_manager(C), win, sa2);

  BKE_screen_area_free(tmp);
  MEM_delete(tmp);

  /* tell WM to refresh, cursor types etc */
  WM_event_add_mousemove(win);

  ED_area_tag_redraw(sa1);
  ED_area_tag_refresh(sa1);
  ED_area_tag_redraw(sa2);
  ED_area_tag_refresh(sa2);
}

void ED_area_newspace(bContext *C, ScrArea *area, int type, const bool skip_region_exit)
{
  wmWindow *win = CTX_wm_window(C);

  if (area->spacetype != type) {
    SpaceLink *slold = static_cast<SpaceLink *>(area->spacedata.first);
    /* store area->type->exit callback */
    void (*area_exit)(wmWindowManager *, ScrArea *) = area->type ? area->type->exit : nullptr;
    /* When the user switches between space-types from the type-selector,
     * changing the header-type is jarring (especially when using Ctrl-MouseWheel).
     *
     * However, add-on install for example, forces the header to the top which shouldn't
     * be applied back to the previous space type when closing - see: #57724
     *
     * Newly-created windows won't have any space data, use the alignment
     * the space type defaults to in this case instead
     * (needed for preferences to have space-type on bottom).
     */

    bool sync_header_alignment = false;
    RegionTypeAlignInfo region_align_info[RGN_TYPE_NUM];
    if ((slold != nullptr) && (slold->link_flag & SPACE_FLAG_TYPE_TEMPORARY) == 0) {
      region_align_info_from_area(area, region_align_info);
      sync_header_alignment = true;
    }

    /* in some cases (opening temp space) we don't want to
     * call area exit callback, so we temporarily unset it */
    if (skip_region_exit && area->type) {
      area->type->exit = nullptr;
    }

    ED_area_exit(C, area);

    /* restore old area exit callback */
    if (skip_region_exit && area->type) {
      area->type->exit = area_exit;
    }

    SpaceType *st = BKE_spacetype_from_id(type);

    area->spacetype = type;
    area->type = st;

    /* If st->create may be called, don't use context until then. The
     * area->type->context() callback has changed but data may be invalid
     * (e.g. with properties editor) until space-data is properly created */

    /* check previously stored space */
    SpaceLink *sl = nullptr;
    LISTBASE_FOREACH (SpaceLink *, sl_iter, &area->spacedata) {
      if (sl_iter->spacetype == type) {
        sl = sl_iter;
        break;
      }
    }

    /* old spacedata... happened during work on 2.50, remove */
    if (sl && BLI_listbase_is_empty(&sl->regionbase)) {
      st->free(sl);
      BLI_freelinkN(&area->spacedata, sl);
      if (slold == sl) {
        slold = nullptr;
      }
      sl = nullptr;
    }

    if (sl) {
      /* swap regions */
      slold->regionbase = area->regionbase;
      area->regionbase = sl->regionbase;
      BLI_listbase_clear(&sl->regionbase);
      /* SPACE_FLAG_TYPE_WAS_ACTIVE is only used to go back to a previously active space that is
       * overlapped by temporary ones. It's now properly activated, so the flag should be cleared
       * at this point. */
      sl->link_flag &= ~SPACE_FLAG_TYPE_WAS_ACTIVE;

      /* put in front of list */
      BLI_remlink(&area->spacedata, sl);
      BLI_addhead(&area->spacedata, sl);
    }
    else {
      /* new space */
      if (st) {
        /* Don't get scene from context here which may depend on space-data. */
        Scene *scene = WM_window_get_active_scene(win);
        sl = st->create(area, scene);
        BLI_addhead(&area->spacedata, sl);

        /* swap regions */
        if (slold) {
          slold->regionbase = area->regionbase;
        }
        area->regionbase = sl->regionbase;
        BLI_listbase_clear(&sl->regionbase);
      }
    }

    /* Sync header alignment. */
    if (sync_header_alignment) {
      region_align_info_to_area(area, region_align_info);
    }

    ED_area_init(CTX_wm_manager(C), win, area);

    /* tell WM to refresh, cursor types etc */
    WM_event_add_mousemove(win);

    /* send space change notifier */
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_CHANGED, area);

    ED_area_tag_refresh(area);
  }

  /* also redraw when re-used */
  ED_area_tag_redraw(area);
}

static SpaceLink *area_get_prevspace(ScrArea *area)
{
  SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);

  /* First toggle to the next temporary space in the list. */
  for (SpaceLink *sl_iter = sl->next; sl_iter; sl_iter = sl_iter->next) {
    if (sl_iter->link_flag & SPACE_FLAG_TYPE_TEMPORARY) {
      return sl_iter;
    }
  }

  /* No temporary space, find the item marked as last active. */
  for (SpaceLink *sl_iter = sl->next; sl_iter; sl_iter = sl_iter->next) {
    if (sl_iter->link_flag & SPACE_FLAG_TYPE_WAS_ACTIVE) {
      return sl_iter;
    }
  }

  /* If neither is found, we can just return to the regular previous one. */
  return sl->next;
}

void ED_area_prevspace(bContext *C, ScrArea *area)
{
  SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
  SpaceLink *prevspace = sl ? area_get_prevspace(area) : nullptr;

  if (prevspace) {
    ED_area_newspace(C, area, prevspace->spacetype, false);
    /* We've exited the space, so it can't be considered temporary anymore. */
    sl->link_flag &= ~SPACE_FLAG_TYPE_TEMPORARY;
  }
  else {
    /* no change */
    return;
  }
  /* If this is a stacked full-screen, changing to previous area exits it (meaning we're still in a
   * full-screen, but not in a stacked one). */
  area->flag &= ~AREA_FLAG_STACKED_FULLSCREEN;

  ED_area_tag_redraw(area);

  /* send space change notifier */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_CHANGED, area);
}

int ED_area_header_switchbutton(const bContext *C, uiBlock *block, int yco)
{
  ScrArea *area = CTX_wm_area(C);
  bScreen *screen = CTX_wm_screen(C);
  int xco = 0.4 * U.widget_unit;

  PointerRNA areaptr = RNA_pointer_create(&(screen->id), &RNA_Area, area);

  uiDefButR(block,
            UI_BTYPE_MENU,
            0,
            "",
            xco,
            yco,
            1.6 * U.widget_unit,
            U.widget_unit,
            &areaptr,
            "ui_type",
            0,
            0.0f,
            0.0f,
            "");

  return xco + 1.7 * U.widget_unit;
}

/************************ standard UI regions ************************/

static ThemeColorID region_background_color_id(const bContext *C, const ARegion *region)
{
  ScrArea *area = CTX_wm_area(C);

  switch (region->regiontype) {
    case RGN_TYPE_HEADER:
    case RGN_TYPE_TOOL_HEADER:
      if (ED_screen_area_active(C) && !ED_area_is_global(area)) {
        return TH_HEADER_ACTIVE;
      }
      else {
        return TH_HEADER;
      }
    case RGN_TYPE_PREVIEW:
      return TH_PREVIEW_BACK;
    default:
      return TH_BACK;
  }
}

void ED_region_clear(const bContext *C, const ARegion *region, const int /*ThemeColorID*/ colorid)
{
  if (region->overlap) {
    /* view should be in pixelspace */
    UI_view2d_view_restore(C);

    float back[4];
    UI_GetThemeColor4fv(colorid, back);
    GPU_clear_color(back[3] * back[0], back[3] * back[1], back[3] * back[2], back[3]);
  }
  else {
    UI_ThemeClearColor(colorid);
  }
}

static void region_clear_fully_transparent(const bContext *C)
{
  /* view should be in pixelspace */
  UI_view2d_view_restore(C);

  GPU_clear_color(0, 0, 0, 0);
}

BLI_INLINE bool streq_array_any(const char *s, const char *arr[])
{
  for (uint i = 0; arr[i]; i++) {
    if (STREQ(arr[i], s)) {
      return true;
    }
  }
  return false;
}

/**
 * Builds the panel layout for the input \a panel or type \a pt.
 *
 * \param panel: The panel to draw. Can be null,
 * in which case a panel with the type of \a pt will be created.
 * \param unique_panel_str: A unique identifier for the name of the \a uiBlock associated with the
 * panel. Used when the panel is an instanced panel so a unique identifier is needed to find the
 * correct old \a uiBlock, and nullptr otherwise.
 */
static void ed_panel_draw(const bContext *C,
                          ARegion *region,
                          ListBase *lb,
                          PanelType *pt,
                          Panel *panel,
                          int w,
                          int em,
                          char *unique_panel_str,
                          const char *search_filter,
                          wmOperatorCallContext op_context)
{
  const uiStyle *style = UI_style_get_dpi();

  /* Draw panel. */
  char block_name[BKE_ST_MAXNAME + INSTANCED_PANEL_UNIQUE_STR_SIZE];
  if (unique_panel_str) {
    /* Instanced panels should have already been added at this point. */
    BLI_string_join(block_name, sizeof(block_name), pt->idname, unique_panel_str);
  }
  else {
    STRNCPY(block_name, pt->idname);
  }
  uiBlock *block = UI_block_begin(C, region, block_name, UI_EMBOSS);

  bool open;
  panel = UI_panel_begin(region, lb, block, pt, panel, &open);
  panel->runtime->layout_panels.clear();

  const bool search_filter_active = search_filter != nullptr && search_filter[0] != '\0';

  /* bad fixed values */
  int xco, yco, h = 0;
  int headerend = w - UI_UNIT_X;

  UI_panel_header_buttons_begin(panel);
  if (pt->draw_header_preset && !(pt->flag & PANEL_TYPE_NO_HEADER)) {
    /* for preset menu */
    panel->layout = UI_block_layout(block,
                                    UI_LAYOUT_HORIZONTAL,
                                    UI_LAYOUT_HEADER,
                                    0,
                                    (UI_UNIT_Y * 1.1f) + style->panelspace,
                                    UI_UNIT_Y,
                                    1,
                                    0,
                                    style);

    uiLayoutSetOperatorContext(panel->layout, op_context);

    pt->draw_header_preset(C, panel);

    UI_block_apply_search_filter(block, search_filter);
    UI_block_layout_resolve(block, &xco, &yco);
    UI_block_translate(block, headerend - xco, 0);
    panel->layout = nullptr;
  }

  if (pt->draw_header && !(pt->flag & PANEL_TYPE_NO_HEADER)) {
    int labelx, labely;
    UI_panel_label_offset(block, &labelx, &labely);

    /* Unusual case: Use expanding layout (buttons stretch to available width). */
    if (pt->flag & PANEL_TYPE_HEADER_EXPAND) {
      uiLayout *layout = UI_block_layout(block,
                                         UI_LAYOUT_VERTICAL,
                                         UI_LAYOUT_PANEL,
                                         labelx,
                                         labely,
                                         headerend - 2 * style->panelspace,
                                         1,
                                         0,
                                         style);
      panel->layout = uiLayoutRow(layout, false);
    }
    /* Regular case: Normal panel with fixed size buttons. */
    else {
      panel->layout = UI_block_layout(
          block, UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER, labelx, labely, UI_UNIT_Y, 1, 0, style);
    }

    uiLayoutSetOperatorContext(panel->layout, op_context);

    pt->draw_header(C, panel);

    UI_block_apply_search_filter(block, search_filter);
    UI_block_layout_resolve(block, &xco, &yco);
    panel->labelofs = xco - labelx;
    panel->layout = nullptr;
  }
  else {
    panel->labelofs = 0;
  }
  UI_panel_header_buttons_end(panel);

  if (open || search_filter_active) {
    short panelContext;

    /* panel context can either be toolbar region or normal panels region */
    if (pt->flag & PANEL_TYPE_LAYOUT_VERT_BAR) {
      panelContext = UI_LAYOUT_VERT_BAR;
    }
    else if (region->regiontype == RGN_TYPE_TOOLS) {
      panelContext = UI_LAYOUT_TOOLBAR;
    }
    else {
      panelContext = UI_LAYOUT_PANEL;
    }

    panel->layout = UI_block_layout(
        block,
        UI_LAYOUT_VERTICAL,
        panelContext,
        (pt->flag & PANEL_TYPE_LAYOUT_VERT_BAR) ? 0 : style->panelspace,
        0,
        (pt->flag & PANEL_TYPE_LAYOUT_VERT_BAR) ? 0 : w - 2 * style->panelspace,
        em,
        0,
        style);

    uiLayoutSetOperatorContext(panel->layout, op_context);

    pt->draw(C, panel);

    const bool ends_with_layout_panel_header = uiLayoutEndsWithPanelHeader(*panel->layout);

    UI_block_apply_search_filter(block, search_filter);
    UI_block_layout_resolve(block, &xco, &yco);
    panel->layout = nullptr;

    if (yco != 0) {
      h = -yco;
      h += style->panelspace;
      if (!ends_with_layout_panel_header) {
        /* Last layout panel header ends together with the panel. */
        h += style->panelspace;
      }
    }
  }

  UI_block_end(C, block);

  /* Draw child panels. */
  if (open || search_filter_active) {
    LISTBASE_FOREACH (LinkData *, link, &pt->children) {
      PanelType *child_pt = static_cast<PanelType *>(link->data);
      Panel *child_panel = UI_panel_find_by_type(&panel->children, child_pt);

      if (child_pt->draw && (!child_pt->poll || child_pt->poll(C, child_pt))) {
        ed_panel_draw(C,
                      region,
                      &panel->children,
                      child_pt,
                      child_panel,
                      w,
                      em,
                      unique_panel_str,
                      search_filter,
                      op_context);
      }
    }
  }

  UI_panel_end(panel, w, h);
}

/**
 * Check whether a panel should be added to the region's panel layout.
 */
static bool panel_add_check(const bContext *C,
                            const WorkSpace *workspace,
                            const char *contexts[],
                            const char *category_override,
                            PanelType *panel_type)
{
  /* Only add top level panels. */
  if (panel_type->parent) {
    return false;
  }
  /* Check the category override first. */
  if (category_override) {
    if (!STREQ(panel_type->category, category_override)) {
      return false;
    }
  }

  /* Verify context. */
  if (contexts != nullptr && panel_type->context[0]) {
    if (!streq_array_any(panel_type->context, contexts)) {
      return false;
    }
  }

  /* If we're tagged, only use compatible. */
  if (panel_type->owner_id[0]) {
    if (!BKE_workspace_owner_id_check(workspace, panel_type->owner_id)) {
      return false;
    }
  }

  if (LIKELY(panel_type->draw)) {
    if (panel_type->poll && !panel_type->poll(C, panel_type)) {
      return false;
    }
  }

  return true;
}

static bool region_uses_category_tabs(const ScrArea *area, const ARegion *region)
{
  /* XXX, should use some better check? */
  /* For now also has hardcoded check for clip editor until it supports actual toolbar. */
  return ((1 << region->regiontype) & RGN_TYPE_HAS_CATEGORY_MASK) ||
         (region->regiontype == RGN_TYPE_TOOLS && area->spacetype == SPACE_CLIP);
}

static const char *region_panels_collect_categories(ARegion *region,
                                                    LinkNode *panel_types_stack,
                                                    bool *use_category_tabs)
{
  UI_panel_category_clear_all(region);

  /* gather unique categories */
  for (LinkNode *pt_link = panel_types_stack; pt_link; pt_link = pt_link->next) {
    PanelType *pt = static_cast<PanelType *>(pt_link->link);
    if (pt->category[0]) {
      if (!UI_panel_category_find(region, pt->category)) {
        UI_panel_category_add(region, pt->category);
      }
    }
  }

  if (UI_panel_category_is_visible(region)) {
    return UI_panel_category_active_get(region, true);
  }

  *use_category_tabs = false;
  return nullptr;
}

static int panel_draw_width_from_max_width_get(const ARegion *region,
                                               const PanelType *panel_type,
                                               const int max_width)
{
  /* With a background, we want some extra padding. */
  return UI_panel_should_show_background(region, panel_type) ?
             max_width - UI_PANEL_MARGIN_X * 2.0f :
             max_width;
}

void ED_region_panels_layout_ex(const bContext *C,
                                ARegion *region,
                                ListBase *paneltypes,
                                wmOperatorCallContext op_context,
                                const char *contexts[],
                                const char *category_override)
{
  /* collect panels to draw */
  WorkSpace *workspace = CTX_wm_workspace(C);
  LinkNode *panel_types_stack = nullptr;
  LISTBASE_FOREACH_BACKWARD (PanelType *, pt, paneltypes) {
    if (panel_add_check(C, workspace, contexts, category_override, pt)) {
      BLI_linklist_prepend_alloca(&panel_types_stack, pt);
    }
  }

  region->runtime.category = nullptr;

  ScrArea *area = CTX_wm_area(C);
  View2D *v2d = &region->v2d;

  bool use_category_tabs = (category_override == nullptr) &&
                           region_uses_category_tabs(area, region);
  /* offset panels for small vertical tab area */
  const char *category = nullptr;
  const int category_tabs_width = UI_PANEL_CATEGORY_MARGIN_WIDTH;
  int margin_x = 0;
  const bool region_layout_based = region->flag & RGN_FLAG_DYNAMIC_SIZE;
  bool update_tot_size = true;

  /* only allow scrolling in vertical direction */
  v2d->keepofs |= V2D_LOCKOFS_X | V2D_KEEPOFS_Y;
  v2d->keepofs &= ~(V2D_LOCKOFS_Y | V2D_KEEPOFS_X);
  v2d->scroll &= ~V2D_SCROLL_BOTTOM;
  v2d->scroll |= V2D_SCROLL_RIGHT;

  /* collect categories */
  if (use_category_tabs) {
    category = region_panels_collect_categories(region, panel_types_stack, &use_category_tabs);
  }
  if (use_category_tabs) {
    margin_x = category_tabs_width;
  }

  const int max_panel_width = BLI_rctf_size_x(&v2d->cur) - margin_x;
  /* Works out to 10 * UI_UNIT_X or 20 * UI_UNIT_X. */
  const int em = (region->type->prefsizex) ? 10 : 20;

  /* create panels */
  UI_panels_begin(C, region);

  /* Get search string for property search. */
  const char *search_filter = ED_area_region_search_filter_get(area, region);

  /* set view2d view matrix  - UI_block_begin() stores it */
  UI_view2d_view_ortho(v2d);

  bool has_instanced_panel = false;
  for (LinkNode *pt_link = panel_types_stack; pt_link; pt_link = pt_link->next) {
    PanelType *pt = static_cast<PanelType *>(pt_link->link);

    if (pt->flag & PANEL_TYPE_INSTANCED) {
      has_instanced_panel = true;
      continue;
    }
    Panel *panel = UI_panel_find_by_type(&region->panels, pt);

    if (use_category_tabs && pt->category[0] && !STREQ(category, pt->category)) {
      if ((panel == nullptr) || ((panel->flag & PNL_PIN) == 0)) {
        continue;
      }
    }
    const int width = panel_draw_width_from_max_width_get(region, pt, max_panel_width);

    if (panel && UI_panel_is_dragging(panel)) {
      /* Prevent View2d.tot rectangle size changes while dragging panels. */
      update_tot_size = false;
    }

    ed_panel_draw(
        C, region, &region->panels, pt, panel, width, em, nullptr, search_filter, op_context);
  }

  /* Draw "poly-instantiated" panels that don't have a 1 to 1 correspondence with their types. */
  if (has_instanced_panel) {
    LISTBASE_FOREACH (Panel *, panel, &region->panels) {
      if (panel->type == nullptr) {
        continue; /* Some panels don't have a type. */
      }
      if (!(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        continue;
      }
      if (use_category_tabs && panel->type->category[0] && !STREQ(category, panel->type->category))
      {
        continue;
      }
      const int width = panel_draw_width_from_max_width_get(region, panel->type, max_panel_width);

      if (UI_panel_is_dragging(panel)) {
        /* Prevent View2d.tot rectangle size changes while dragging panels. */
        update_tot_size = false;
      }

      /* Use a unique identifier for instanced panels, otherwise an old block for a different
       * panel of the same type might be found. */
      char unique_panel_str[INSTANCED_PANEL_UNIQUE_STR_SIZE];
      UI_list_panel_unique_str(panel, unique_panel_str);
      ed_panel_draw(C,
                    region,
                    &region->panels,
                    panel->type,
                    panel,
                    width,
                    em,
                    unique_panel_str,
                    search_filter,
                    op_context);
    }
  }

  /* align panels and return size */
  int x, y;
  UI_panels_end(C, region, &x, &y);

  /* before setting the view */
  if (region_layout_based) {
    /* XXX, only single panel support at the moment.
     * Can't use x/y values calculated above because they're not using the real height of panels,
     * instead they calculate offsets for the next panel to start drawing. */
    Panel *panel = static_cast<Panel *>(region->panels.last);
    if (panel != nullptr) {
      const int size_dyn[2] = {
          int(UI_UNIT_X * (UI_panel_is_closed(panel) ? 8 : 14) / UI_SCALE_FAC),
          int(UI_panel_size_y(panel) / UI_SCALE_FAC),
      };
      /* region size is layout based and needs to be updated */
      if ((region->sizex != size_dyn[0]) || (region->sizey != size_dyn[1])) {
        region->sizex = size_dyn[0];
        region->sizey = size_dyn[1];
        ED_area_tag_region_size_update(area, region);
      }
      y = fabsf(region->sizey * UI_SCALE_FAC - 1);
    }
  }
  else {
    /* We always keep the scroll offset -
     * so the total view gets increased with the scrolled away part. */
    if (v2d->cur.ymax < -FLT_EPSILON) {
      /* Clamp to lower view boundary */
      if (v2d->tot.ymin < -v2d->winy) {
        y = min_ii(y, 0);
      }
      else {
        y = min_ii(y, v2d->cur.ymin);
      }
    }

    y = -y;
  }

  UI_blocklist_update_view_for_buttons(C, &region->uiblocks);

  if (update_tot_size) {
    /* this also changes the 'cur' */
    UI_view2d_totRect_set(v2d, x, y);
  }

  if (use_category_tabs) {
    region->runtime.category = category;
  }
}

void ED_region_panels_layout(const bContext *C, ARegion *region)
{
  ED_region_panels_layout_ex(
      C, region, &region->type->paneltypes, WM_OP_INVOKE_REGION_WIN, nullptr, nullptr);
}

void ED_region_panels_draw(const bContext *C, ARegion *region)
{
  View2D *v2d = &region->v2d;

  if (region->alignment != RGN_ALIGN_FLOAT) {
    ED_region_clear(
        C, region, (region->type->regionid == RGN_TYPE_PREVIEW) ? TH_PREVIEW_BACK : TH_BACK);
  }

  /* reset line width for drawing tabs */
  GPU_line_width(1.0f);

  /* set the view */
  UI_view2d_view_ortho(v2d);

  /* View2D matrix might have changed due to dynamic sized regions. */
  UI_blocklist_update_window_matrix(C, &region->uiblocks);

  /* draw panels */
  UI_panels_draw(C, region);

  /* restore view matrix */
  UI_view2d_view_restore(C);

  /* Set in layout. */
  if (region->runtime.category) {
    UI_panel_category_draw_all(region, region->runtime.category);
  }

  /* scrollers */
  bool use_mask = false;
  rcti mask;
  if (region->runtime.category &&
      (RGN_ALIGN_ENUM_FROM_MASK(region->alignment) == RGN_ALIGN_RIGHT) &&
      UI_panel_category_is_visible(region))
  {
    use_mask = true;
    UI_view2d_mask_from_win(v2d, &mask);
    mask.xmax -= round_fl_to_int(UI_view2d_scale_get_x(&region->v2d) *
                                 UI_PANEL_CATEGORY_MARGIN_WIDTH);
  }
  bool use_full_hide = false;
  if (region->overlap) {
    /* Don't always show scrollbars for transparent regions as it's distracting. */
    use_full_hide = true;
  }
  UI_view2d_scrollers_draw_ex(v2d, use_mask ? &mask : nullptr, use_full_hide);
}

void ED_region_panels_ex(const bContext *C,
                         ARegion *region,
                         wmOperatorCallContext op_context,
                         const char *contexts[])
{
  /* TODO: remove? */
  ED_region_panels_layout_ex(C, region, &region->type->paneltypes, op_context, contexts, nullptr);
  ED_region_panels_draw(C, region);
}

void ED_region_panels(const bContext *C, ARegion *region)
{
  /* TODO: remove? */
  ED_region_panels_layout(C, region);
  ED_region_panels_draw(C, region);
}

void ED_region_panels_init(wmWindowManager *wm, ARegion *region)
{
  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_PANELS_UI, region->winx, region->winy);

  wmKeyMap *keymap = WM_keymap_ensure(
      wm->defaultconf, "View2D Buttons List", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);
}

/**
 * Check whether any of the buttons generated by the \a panel_type's
 * layout callbacks match the \a search_filter.
 *
 * \param panel: If non-null, use this instead of adding a new panel for the \a panel_type.
 */
static bool panel_property_search(const bContext *C,
                                  ARegion *region,
                                  const uiStyle *style,
                                  Panel *panel,
                                  PanelType *panel_type,
                                  const char *search_filter)
{
  uiBlock *block = UI_block_begin(C, region, panel_type->idname, UI_EMBOSS);
  UI_block_set_search_only(block, true);

  /* Skip panels that give meaningless search results. */
  if (panel_type->flag & PANEL_TYPE_NO_SEARCH) {
    return false;
  }

  if (panel == nullptr) {
    bool open; /* Dummy variable. */
    panel = UI_panel_begin(region, &region->panels, block, panel_type, panel, &open);
  }

  /* Build the layouts. Because they are only used for search,
   * they don't need any of the proper style or layout information. */
  if (panel->type->draw_header_preset != nullptr) {
    panel->layout = UI_block_layout(
        block, UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER, 0, 0, 0, 0, 0, style);
    panel_type->draw_header_preset(C, panel);
  }
  if (panel->type->draw_header != nullptr) {
    panel->layout = UI_block_layout(
        block, UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER, 0, 0, 0, 0, 0, style);
    panel_type->draw_header(C, panel);
  }
  if (LIKELY(panel->type->draw != nullptr)) {
    panel->layout = UI_block_layout(
        block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, 0, 0, 0, style);
    panel_type->draw(C, panel);
  }

  UI_block_layout_free(block);

  /* We could check after each layout to increase the likelihood of returning early,
   * but that probably wouldn't make much of a difference anyway. */
  if (UI_block_apply_search_filter(block, search_filter)) {
    return true;
  }

  LISTBASE_FOREACH (LinkData *, link, &panel_type->children) {
    PanelType *panel_type_child = static_cast<PanelType *>(link->data);
    if (!panel_type_child->poll || panel_type_child->poll(C, panel_type_child)) {
      /* Search for the existing child panel here because it might be an instanced
       * child panel with a custom data field that will be needed to build the layout. */
      Panel *child_panel = UI_panel_find_by_type(&panel->children, panel_type_child);
      if (panel_property_search(C, region, style, child_panel, panel_type_child, search_filter)) {
        return true;
      }
    }
  }

  return false;
}

bool ED_region_property_search(const bContext *C,
                               ARegion *region,
                               ListBase *paneltypes,
                               const char *contexts[],
                               const char *category_override)
{
  ScrArea *area = CTX_wm_area(C);
  WorkSpace *workspace = CTX_wm_workspace(C);
  const uiStyle *style = UI_style_get_dpi();
  const char *search_filter = ED_area_region_search_filter_get(area, region);

  LinkNode *panel_types_stack = nullptr;
  LISTBASE_FOREACH_BACKWARD (PanelType *, pt, paneltypes) {
    if (panel_add_check(C, workspace, contexts, category_override, pt)) {
      BLI_linklist_prepend_alloca(&panel_types_stack, pt);
    }
  }

  const char *category = nullptr;
  bool use_category_tabs = (category_override == nullptr) &&
                           region_uses_category_tabs(area, region);
  if (use_category_tabs) {
    category = region_panels_collect_categories(region, panel_types_stack, &use_category_tabs);
  }

  /* Run property search for each panel, stopping if a result is found. */
  bool has_result = true;
  bool has_instanced_panel = false;
  for (LinkNode *pt_link = panel_types_stack; pt_link; pt_link = pt_link->next) {
    PanelType *panel_type = static_cast<PanelType *>(pt_link->link);
    /* Note that these checks are duplicated from #ED_region_panels_layout_ex. */
    if (panel_type->flag & PANEL_TYPE_INSTANCED) {
      has_instanced_panel = true;
      continue;
    }

    if (use_category_tabs) {
      if (panel_type->category[0] && !STREQ(category, panel_type->category)) {
        continue;
      }
    }

    /* We start property search with an empty panel list, so there's
     * no point in trying to find an existing panel with this type. */
    has_result = panel_property_search(C, region, style, nullptr, panel_type, search_filter);
    if (has_result) {
      break;
    }
  }

  /* Run property search for instanced panels (created in the layout calls of previous panels). */
  if (!has_result && has_instanced_panel) {
    LISTBASE_FOREACH (Panel *, panel, &region->panels) {
      /* Note that these checks are duplicated from #ED_region_panels_layout_ex. */
      if (panel->type == nullptr || !(panel->type->flag & PANEL_TYPE_INSTANCED)) {
        continue;
      }
      if (use_category_tabs) {
        if (panel->type->category[0] && !STREQ(category, panel->type->category)) {
          continue;
        }
      }

      has_result = panel_property_search(C, region, style, panel, panel->type, search_filter);
      if (has_result) {
        break;
      }
    }
  }

  /* Free the panels and blocks, as they are only used for search. */
  UI_blocklist_free(C, region);
  UI_panels_free_instanced(C, region);
  BKE_area_region_panels_free(&region->panels);

  return has_result;
}

/**
 * The drawable part of the region might be slightly smaller because of area edges. This is
 * especially visible for header-like regions, where vertical space around widgets is small, and
 * it's quite visible when widgets aren't centered properly.
 */
static void layout_coordinates_correct_for_drawable_rect(
    const wmWindow *win, const ScrArea *area, const ARegion *region, int * /*r_xco*/, int *r_yco)
{
  /* Don't do corrections at the window borders where the region rectangles are clipped already. */
  {
    rcti win_rect;
    WM_window_rect_calc(win, &win_rect);
    if (region->winrct.ymin == win_rect.ymin) {
      return;
    }
    if (region->winrct.ymax == (win_rect.ymax - 1)) {
      return;
    }
  }

  if (region->winrct.ymax == area->totrct.ymax) {
    *r_yco -= 1;
  }
}

void ED_region_header_layout(const bContext *C, ARegion *region)
{
  const uiStyle *style = UI_style_get_dpi();
  bool region_layout_based = region->flag & RGN_FLAG_DYNAMIC_SIZE;

  /* Height of buttons and scaling needed to achieve it. */
  const int buttony = min_ii(UI_UNIT_Y, region->winy - 2 * UI_SCALE_FAC);
  const float buttony_scale = buttony / float(UI_UNIT_Y);

  /* Vertically center buttons. */
  int xco = UI_HEADER_OFFSET;
  int yco = buttony + (region->winy - buttony) / 2;
  int maxco = xco;

  layout_coordinates_correct_for_drawable_rect(
      CTX_wm_window(C), CTX_wm_area(C), region, &xco, &yco);

  /* set view2d view matrix for scrolling (without scrollers) */
  UI_view2d_view_ortho(&region->v2d);

  /* draw all headers types */
  LISTBASE_FOREACH (HeaderType *, ht, &region->type->headertypes) {
    if (ht->poll && !ht->poll(C, ht)) {
      continue;
    }

    uiBlock *block = UI_block_begin(C, region, ht->idname, UI_EMBOSS);
    uiLayout *layout = UI_block_layout(
        block, UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER, xco, yco, buttony, 1, 0, style);

    if (buttony_scale != 1.0f) {
      uiLayoutSetScaleY(layout, buttony_scale);
    }

    Header header = {nullptr};
    if (ht->draw) {
      header.type = ht;
      header.layout = layout;
      ht->draw(C, &header);
      if (ht->next) {
        uiItemS(layout);
      }

      /* for view2d */
      xco = uiLayoutGetWidth(layout);
      if (xco > maxco) {
        maxco = xco;
      }
    }

    UI_block_layout_resolve(block, &xco, &yco);

    /* for view2d */
    if (xco > maxco) {
      maxco = xco;
    }

    int new_sizex = (maxco + UI_HEADER_OFFSET) / UI_SCALE_FAC;

    if (region_layout_based && (region->sizex != new_sizex)) {
      /* region size is layout based and needs to be updated */
      ScrArea *area = CTX_wm_area(C);

      region->sizex = new_sizex;
      ED_area_tag_region_size_update(area, region);
    }

    UI_block_end(C, block);

    /* In most cases there is only ever one header, it never makes sense to draw more than one
     * header in the same region, this results in overlapping buttons, see: #60195. */
    break;
  }

  if (!region_layout_based) {
    maxco += UI_HEADER_OFFSET;
  }

  /* Always as last. */
  UI_view2d_totRect_set(&region->v2d, maxco, region->winy);

  /* Restore view matrix. */
  UI_view2d_view_restore(C);
}

static void region_draw_blocks_in_view2d(const bContext *C, const ARegion *region)
{
  UI_view2d_view_ortho(&region->v2d);

  /* View2D matrix might have changed due to dynamic sized regions. */
  UI_blocklist_update_window_matrix(C, &region->uiblocks);

  /* draw blocks */
  UI_blocklist_draw(C, &region->uiblocks);

  /* restore view matrix */
  UI_view2d_view_restore(C);
}

void ED_region_header_draw(const bContext *C, ARegion *region)
{
  /* clear */
  ED_region_clear(C, region, region_background_color_id(C, region));
  region_draw_blocks_in_view2d(C, region);
}

void ED_region_header_draw_with_button_sections(const bContext *C,
                                                const ARegion *region,
                                                const uiButtonSectionsAlign align)
{
  const ThemeColorID bgcolorid = region_background_color_id(C, region);

  /* Clear and draw button sections background when using region overlap. Otherwise clear using the
   * background color like normal. */
  if (region->overlap) {
    region_clear_fully_transparent(C);
    UI_region_button_sections_draw(region, bgcolorid, align);
  }
  else {
    ED_region_clear(C, region, bgcolorid);
  }
  region_draw_blocks_in_view2d(C, region);
}

void ED_region_header(const bContext *C, ARegion *region)
{
  /* TODO: remove? */
  ED_region_header_layout(C, region);
  ED_region_header_draw(C, region);
}

void ED_region_header_with_button_sections(const bContext *C,
                                           ARegion *region,
                                           const uiButtonSectionsAlign align)
{
  ED_region_header_layout(C, region);
  ED_region_header_draw_with_button_sections(C, region, align);
}

void ED_region_header_init(ARegion *region)
{
  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_HEADER, region->winx, region->winy);
}

int ED_area_headersize()
{
  /* Accommodate widget and padding. */
  return U.widget_unit + int(UI_SCALE_FAC * HEADER_PADDING_Y);
}

int ED_area_footersize()
{
  return ED_area_headersize();
}

int ED_area_global_size_y(const ScrArea *area)
{
  BLI_assert(ED_area_is_global(area));
  return round_fl_to_int(area->global->cur_fixed_height * UI_SCALE_FAC);
}
int ED_area_global_min_size_y(const ScrArea *area)
{
  BLI_assert(ED_area_is_global(area));
  return round_fl_to_int(area->global->size_min * UI_SCALE_FAC);
}
int ED_area_global_max_size_y(const ScrArea *area)
{
  BLI_assert(ED_area_is_global(area));
  return round_fl_to_int(area->global->size_max * UI_SCALE_FAC);
}

bool ED_area_is_global(const ScrArea *area)
{
  return area->global != nullptr;
}

ScrArea *ED_area_find_under_cursor(const bContext *C, int spacetype, const int event_xy[2])
{
  bScreen *screen = CTX_wm_screen(C);
  wmWindow *win = CTX_wm_window(C);

  ScrArea *area = nullptr;

  if (win->parent) {
    /* If active window is a child, check itself first. */
    area = BKE_screen_find_area_xy(screen, spacetype, event_xy);
  }

  if (!area) {
    /* Check all windows except the active one. */
    int event_xy_other[2];
    wmWindow *win_other = WM_window_find_under_cursor(win, event_xy, event_xy_other);
    if (win_other && win_other != win) {
      win = win_other;
      screen = WM_window_get_active_screen(win);
      area = BKE_screen_find_area_xy(screen, spacetype, event_xy_other);
    }
  }

  if (!area && !win->parent) {
    /* If active window is a parent window, check itself last. */
    area = BKE_screen_find_area_xy(screen, spacetype, event_xy);
  }

  return area;
}

ScrArea *ED_screen_areas_iter_first(const wmWindow *win, const bScreen *screen)
{
  ScrArea *global_area = static_cast<ScrArea *>(win->global_areas.areabase.first);

  if (!global_area) {
    return static_cast<ScrArea *>(screen->areabase.first);
  }
  if ((global_area->global->flag & GLOBAL_AREA_IS_HIDDEN) == 0) {
    return global_area;
  }
  /* Find next visible area. */
  return ED_screen_areas_iter_next(screen, global_area);
}
ScrArea *ED_screen_areas_iter_next(const bScreen *screen, const ScrArea *area)
{
  if (area->global == nullptr) {
    return area->next;
  }

  for (ScrArea *area_iter = area->next; area_iter; area_iter = area_iter->next) {
    if ((area_iter->global->flag & GLOBAL_AREA_IS_HIDDEN) == 0) {
      return area_iter;
    }
  }
  /* No visible next global area found, start iterating over layout areas. */
  return static_cast<ScrArea *>(screen->areabase.first);
}

int ED_region_global_size_y()
{
  return ED_area_headersize(); /* same size as header */
}

void ED_region_info_draw_multiline(ARegion *region,
                                   const char *text_array[],
                                   float fill_color[4],
                                   const bool full_redraw)
{
  const int header_height = UI_UNIT_Y;
  const uiStyle *style = UI_style_get_dpi();
  int fontid = style->widget.uifont_id;
  int scissor[4];
  int num_lines = 0;

  /* background box */
  rcti rect = *ED_region_visible_rect(region);

  /* Needed in case scripts leave the font size at an unexpected value, see: #102213. */
  BLF_size(fontid, style->widget.points * UI_SCALE_FAC);

  /* Box fill entire width or just around text. */
  if (!full_redraw) {
    const char **text = &text_array[0];
    while (*text) {
      rect.xmax = min_ii(rect.xmax,
                         rect.xmin + BLF_width(fontid, *text, BLF_DRAW_STR_DUMMY_MAX) +
                             1.2f * U.widget_unit);
      text++;
      num_lines++;
    }
  }
  /* Just count the line number. */
  else {
    const char **text = &text_array[0];
    while (*text) {
      text++;
      num_lines++;
    }
  }

  rect.ymin = rect.ymax - header_height * num_lines;

  /* setup scissor */
  GPU_scissor_get(scissor);
  GPU_scissor(rect.xmin, rect.ymin, BLI_rcti_size_x(&rect) + 1, BLI_rcti_size_y(&rect) + 1);

  GPU_blend(GPU_BLEND_ALPHA);
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4fv(fill_color);
  immRecti(pos, rect.xmin, rect.ymin, rect.xmax + 1, rect.ymax + 1);
  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);

  /* text */
  UI_FontThemeColor(fontid, TH_TEXT_HI);
  BLF_clipping(fontid, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
  BLF_enable(fontid, BLF_CLIPPING);
  int offset = num_lines - 1;
  {
    const char **text = &text_array[0];
    while (*text) {
      BLF_position(fontid,
                   rect.xmin + 0.6f * U.widget_unit,
                   rect.ymin + 0.3f * U.widget_unit + offset * header_height,
                   0.0f);
      BLF_draw(fontid, *text, BLF_DRAW_STR_DUMMY_MAX);
      text++;
      offset--;
    }
  }

  BLF_disable(fontid, BLF_CLIPPING);

  /* restore scissor as it was before */
  GPU_scissor(scissor[0], scissor[1], scissor[2], scissor[3]);
}

void ED_region_info_draw(ARegion *region,
                         const char *text,
                         float fill_color[4],
                         const bool full_redraw)
{
  const char *text_array[2] = {text, nullptr};
  ED_region_info_draw_multiline(region, text_array, fill_color, full_redraw);
}

struct MetadataPanelDrawContext {
  uiLayout *layout;
};

static void metadata_panel_draw_field(const char *field, const char *value, void *ctx_v)
{
  MetadataPanelDrawContext *ctx = (MetadataPanelDrawContext *)ctx_v;
  uiLayout *row = uiLayoutRow(ctx->layout, false);
  uiItemL(row, field, ICON_NONE);
  uiItemL(row, value, ICON_NONE);
}

void ED_region_image_metadata_panel_draw(ImBuf *ibuf, uiLayout *layout)
{
  MetadataPanelDrawContext ctx;
  ctx.layout = layout;
  IMB_metadata_foreach(ibuf, metadata_panel_draw_field, &ctx);
}

void ED_region_grid_draw(ARegion *region, float zoomx, float zoomy, float x0, float y0)
{
  /* the image is located inside (x0, y0), (x0+1, y0+1) as set by view2d */
  int x1, y1, x2, y2;
  UI_view2d_view_to_region(&region->v2d, x0, y0, &x1, &y1);
  UI_view2d_view_to_region(&region->v2d, x0 + 1.0f, y0 + 1.0f, &x2, &y2);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  float gridcolor[4];
  UI_GetThemeColor4fv(TH_GRID, gridcolor);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  /* To fake alpha-blending, color shading is reduced when alpha is nearing 0. */
  immUniformThemeColorBlendShade(TH_BACK, TH_GRID, gridcolor[3], 20 * gridcolor[3]);
  immRectf(pos, x1, y1, x2, y2);
  immUnbindProgram();

  /* gridsize adapted to zoom level */
  float gridsize = 0.5f * (zoomx + zoomy);
  float gridstep = 1.0f / 32.0f;
  if (gridsize <= 0.0f) {
    return;
  }

  if (gridsize < 1.0f) {
    while (gridsize < 1.0f) {
      gridsize *= 4.0f;
      gridstep *= 4.0f;
    }
  }
  else {
    while (gridsize >= 4.0f) {
      gridsize /= 4.0f;
      gridstep /= 4.0f;
    }
  }

  float blendfac = 0.25f * gridsize - floorf(0.25f * gridsize);
  CLAMP(blendfac, 0.0f, 1.0f);

  int count_fine = 1.0f / gridstep;
  int count_large = 1.0f / (4.0f * gridstep);

  if (count_fine > 0) {
    GPU_vertformat_clear(format);
    pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);
    immBegin(GPU_PRIM_LINES, 4 * count_fine + 4 * count_large);

    float theme_color[3];
    UI_GetThemeColorShade3fv(TH_GRID, int(20.0f * (1.0f - blendfac)), theme_color);
    float fac = 0.0f;

    /* the fine resolution level */
    for (int i = 0; i < count_fine; i++) {
      immAttr3fv(color, theme_color);
      immVertex2f(pos, x1, y1 * (1.0f - fac) + y2 * fac);
      immAttr3fv(color, theme_color);
      immVertex2f(pos, x2, y1 * (1.0f - fac) + y2 * fac);
      immAttr3fv(color, theme_color);
      immVertex2f(pos, x1 * (1.0f - fac) + x2 * fac, y1);
      immAttr3fv(color, theme_color);
      immVertex2f(pos, x1 * (1.0f - fac) + x2 * fac, y2);
      fac += gridstep;
    }

    if (count_large > 0) {
      UI_GetThemeColor3fv(TH_GRID, theme_color);
      fac = 0.0f;

      /* the large resolution level */
      for (int i = 0; i < count_large; i++) {
        immAttr3fv(color, theme_color);
        immVertex2f(pos, x1, y1 * (1.0f - fac) + y2 * fac);
        immAttr3fv(color, theme_color);
        immVertex2f(pos, x2, y1 * (1.0f - fac) + y2 * fac);
        immAttr3fv(color, theme_color);
        immVertex2f(pos, x1 * (1.0f - fac) + x2 * fac, y1);
        immAttr3fv(color, theme_color);
        immVertex2f(pos, x1 * (1.0f - fac) + x2 * fac, y2);
        fac += 4.0f * gridstep;
      }
    }

    immEnd();
    immUnbindProgram();
  }
}

/* If the area has overlapping regions, it returns visible rect for Region *region */
/* rect gets returned in local region coordinates */
static void region_visible_rect_calc(ARegion *region, rcti *rect)
{
  ARegion *region_iter = region;

  /* allow function to be called without area */
  while (region_iter->prev) {
    region_iter = region_iter->prev;
  }

  *rect = region->winrct;

  /* check if a region overlaps with the current one */
  for (; region_iter; region_iter = region_iter->next) {
    if (region != region_iter && region_iter->overlap) {
      if (BLI_rcti_isect(rect, &region_iter->winrct, nullptr)) {
        int alignment = RGN_ALIGN_ENUM_FROM_MASK(region_iter->alignment);

        if (ELEM(alignment, RGN_ALIGN_LEFT, RGN_ALIGN_RIGHT)) {
          /* Overlap left, also check 1 pixel offset (2 regions on one side). */
          if (abs(rect->xmin - region_iter->winrct.xmin) < 2) {
            rect->xmin = region_iter->winrct.xmax;
          }

          /* Overlap right. */
          if (abs(rect->xmax - region_iter->winrct.xmax) < 2) {
            rect->xmax = region_iter->winrct.xmin;
          }
        }
        else if (ELEM(alignment, RGN_ALIGN_TOP, RGN_ALIGN_BOTTOM)) {
          /* Same logic as above for vertical regions. */
          if (abs(rect->ymin - region_iter->winrct.ymin) < 2) {
            rect->ymin = region_iter->winrct.ymax;
          }
          if (abs(rect->ymax - region_iter->winrct.ymax) < 2) {
            rect->ymax = region_iter->winrct.ymin;
          }
        }
        else if (alignment == RGN_ALIGN_FLOAT) {
          /* Skip floating. */
        }
        else {
          BLI_assert_msg(0, "Region overlap with unknown alignment");
        }
      }
    }
  }
  BLI_rcti_translate(rect, -region->winrct.xmin, -region->winrct.ymin);
}

const rcti *ED_region_visible_rect(ARegion *region)
{
  rcti *rect = &region->runtime.visible_rect;
  if (rect->xmin == 0 && rect->ymin == 0 && rect->xmax == 0 && rect->ymax == 0) {
    region_visible_rect_calc(region, rect);
  }
  return rect;
}

/* Cache display helpers */

void ED_region_cache_draw_background(ARegion *region)
{
  /* Local coordinate visible rect inside region, to accommodate overlapping ui. */
  const rcti *rect_visible = ED_region_visible_rect(region);
  const int region_bottom = rect_visible->ymin;

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4ub(128, 128, 255, 64);
  immRecti(pos, 0, region_bottom, region->winx, region_bottom + 8 * UI_SCALE_FAC);
  immUnbindProgram();
}

void ED_region_cache_draw_curfra_label(const int framenr, const float x, const float y)
{
  const uiStyle *style = UI_style_get();
  int fontid = style->widget.uifont_id;
  char numstr[32];
  float font_dims[2] = {0.0f, 0.0f};

  /* frame number */
  BLF_size(fontid, 11.0f * UI_SCALE_FAC);
  SNPRINTF(numstr, "%d", framenr);

  BLF_width_and_height(fontid, numstr, sizeof(numstr), &font_dims[0], &font_dims[1]);

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColor(TH_CFRAME);
  immRecti(pos, x, y, x + font_dims[0] + 6.0f, y + font_dims[1] + 4.0f);
  immUnbindProgram();

  UI_FontThemeColor(fontid, TH_TEXT);
  BLF_position(fontid, x + 2.0f, y + 2.0f, 0.0f);
  BLF_draw(fontid, numstr, sizeof(numstr));
}

void ED_region_cache_draw_cached_segments(
    ARegion *region, const int num_segments, const int *points, const int sfra, const int efra)
{
  if (num_segments) {
    /* Local coordinate visible rect inside region, to accommodate overlapping ui. */
    const rcti *rect_visible = ED_region_visible_rect(region);
    const int region_bottom = rect_visible->ymin;

    uint pos = GPU_vertformat_attr_add(
        immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformColor4ub(128, 128, 255, 128);

    for (int a = 0; a < num_segments; a++) {
      float x1 = float(points[a * 2] - sfra) / (efra - sfra + 1) * region->winx;
      float x2 = float(points[a * 2 + 1] - sfra + 1) / (efra - sfra + 1) * region->winx;

      immRecti(pos, x1, region_bottom, x2, region_bottom + 8 * UI_SCALE_FAC);
      /* TODO(merwin): use primitive restart to draw multiple rects more efficiently */
    }

    immUnbindProgram();
  }
}

void ED_region_message_subscribe(wmRegionMessageSubscribeParams *params)
{
  ARegion *region = params->region;
  const bContext *C = params->context;
  wmMsgBus *mbus = params->message_bus;

  if (region->gizmo_map != nullptr) {
    WM_gizmomap_message_subscribe(C, region->gizmo_map, region, mbus);
  }

  if (!BLI_listbase_is_empty(&region->uiblocks)) {
    UI_region_message_subscribe(region, mbus);
  }

  if (region->type->message_subscribe != nullptr) {
    region->type->message_subscribe(params);
  }
}

int ED_region_snap_size_test(const ARegion *region)
{
  /* Use a larger value because toggling scrollbars can jump in size. */
  const int snap_match_threshold = 16;
  if (region->type->snap_size != nullptr) {
    const int snap_size_x = region->type->snap_size(region, region->sizex, 0);
    const int snap_size_y = region->type->snap_size(region, region->sizey, 1);
    return (((abs(region->sizex - snap_size_x) <= snap_match_threshold) << 0) |
            ((abs(region->sizey - snap_size_y) <= snap_match_threshold) << 1));
  }
  return 0;
}

bool ED_region_snap_size_apply(ARegion *region, int snap_flag)
{
  bool changed = false;
  if (region->type->snap_size != nullptr) {
    if (snap_flag & (1 << 0)) {
      short snap_size = region->type->snap_size(region, region->sizex, 0);
      if (snap_size != region->sizex) {
        region->sizex = snap_size;
        changed = true;
      }
    }
    if (snap_flag & (1 << 1)) {
      short snap_size = region->type->snap_size(region, region->sizey, 1);
      if (snap_size != region->sizey) {
        region->sizey = snap_size;
        changed = true;
      }
    }
  }
  return changed;
}
