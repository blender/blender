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
 * \ingroup edscr
 */

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_screen_types.h"
#include "ED_space_api.h"
#include "ED_time_scrub_ui.h"

#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "BLF_api.h"

#include "IMB_imbuf_types.h"
#include "IMB_metadata.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "screen_intern.h"

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
  rcti rect;

  /* translate scissor rect to region space */
  rect.xmin = scirct->xmin - region->winrct.xmin;
  rect.ymin = scirct->ymin - region->winrct.ymin;
  rect.xmax = scirct->xmax - region->winrct.xmin;
  rect.ymax = scirct->ymax - region->winrct.ymin;

  /* set transp line */
  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  float color[4] = {0.0f, 0.0f, 0.0f, 0.25f};
  UI_GetThemeColor3fv(TH_EDITOR_OUTLINE, color);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
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

  GPU_blend(false);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void ED_region_pixelspace(ARegion *region)
{
  wmOrtho2_region_pixelspace(region);
  GPU_matrix_identity_set();
}

/* only exported for WM */
void ED_region_do_listen(
    wmWindow *win, ScrArea *area, ARegion *region, wmNotifier *note, const Scene *scene)
{
  /* generic notes first */
  switch (note->category) {
    case NC_WM:
      if (note->data == ND_FILEREAD) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_WINDOW:
      ED_region_tag_redraw(region);
      break;
  }

  if (region->type && region->type->listener) {
    region->type->listener(win, area, region, note, scene);
  }
}

/* only exported for WM */
void ED_area_do_listen(wmWindow *win, ScrArea *area, wmNotifier *note, Scene *scene)
{
  /* no generic notes? */
  if (area->type && area->type->listener) {
    area->type->listener(win, area, note, scene);
  }
}

/* only exported for WM */
void ED_area_do_refresh(bContext *C, ScrArea *area)
{
  /* no generic notes? */
  if (area->type && area->type->refresh) {
    area->type->refresh(C, area);
  }
  area->do_refresh = false;
}

/**
 * \brief Corner widget use for quitting fullscreen.
 */
static void area_draw_azone_fullscreen(
    short UNUSED(x1), short UNUSED(y1), short x2, short y2, float alpha)
{
  UI_icon_draw_ex(x2 - U.widget_unit,
                  y2 - U.widget_unit,
                  ICON_FULLSCREEN_EXIT,
                  U.inv_dpi_fac,
                  min_ff(alpha, 0.75f),
                  0.0f,
                  NULL,
                  false);
}

/**
 * \brief Corner widgets use for dragging and splitting the view.
 */
static void area_draw_azone(short UNUSED(x1), short UNUSED(y1), short UNUSED(x2), short UNUSED(y2))
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

  GPU_blend(true);
  /* NOTE(fclem): There is something strange going on with Mesa and GPU_SHADER_2D_UNIFORM_COLOR
   * that causes a crash on some GPUs (see T76113). Using 3D variant avoid the issue. */
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
  GPU_blend(false);
}

static void region_draw_azone_tab_arrow(ScrArea *area, ARegion *region, AZone *az)
{
  GPU_blend(true);

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
  float color[4] = {0.05f, 0.05f, 0.05f, alpha};
  UI_draw_roundbox_aa(
      true, (float)az->x1, (float)az->y1, (float)az->x2, (float)az->y2, 4.0f, color);

  draw_azone_arrow((float)az->x1, (float)az->y1, (float)az->x2, (float)az->y2, az->edge);
}

static void area_azone_tag_update(ScrArea *area)
{
  area->flag |= AREA_FLAG_ACTIONZONES_UPDATE;
}

static void region_draw_azones(ScrArea *area, ARegion *region)
{
  AZone *az;

  if (!area) {
    return;
  }

  GPU_line_width(1.0f);
  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

  GPU_matrix_push();
  GPU_matrix_translate_2f(-region->winrct.xmin, -region->winrct.ymin);

  for (az = area->actionzones.first; az; az = az->next) {
    /* test if action zone is over this region */
    rcti azrct;
    BLI_rcti_init(&azrct, az->x1, az->x2, az->y1, az->y2);

    if (BLI_rcti_isect(&region->drawrct, &azrct, NULL)) {
      if (az->type == AZONE_AREA) {
        area_draw_azone(az->x1, az->y1, az->x2, az->y2);
      }
      else if (az->type == AZONE_REGION) {
        if (az->region) {
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

  GPU_blend(false);
}

static void region_draw_status_text(ScrArea *area, ARegion *region)
{
  bool overlap = ED_region_is_overlap(area->spacetype, region->regiontype);

  if (overlap) {
    GPU_clear_color(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  else {
    UI_ThemeClearColor(TH_HEADER);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  int fontid = BLF_set_default();

  const float width = BLF_width(fontid, region->headerstr, BLF_DRAW_STR_DUMMY_MAX);
  const float x = UI_UNIT_X;
  const float y = 0.4f * UI_UNIT_Y;

  if (overlap) {
    const float pad = 2.0f * UI_DPI_FAC;
    const float x1 = x - (UI_UNIT_X - pad);
    const float x2 = x + width + (UI_UNIT_X - pad);
    const float y1 = pad;
    const float y2 = region->winy - pad;

    GPU_blend_set_func_separate(
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

    float color[4] = {0.0f, 0.0f, 0.0f, 0.5f};
    UI_GetThemeColor3fv(TH_BACK, color);
    UI_draw_roundbox_corner_set(UI_CNR_ALL);
    UI_draw_roundbox_aa(true, x1, y1, x2, y2, 4.0f, color);

    UI_FontThemeColor(fontid, TH_TEXT);
  }
  else {
    UI_FontThemeColor(fontid, TH_TEXT);
  }

  BLF_position(fontid, x, y, 0.0f);
  BLF_draw(fontid, region->headerstr, BLF_DRAW_STR_DUMMY_MAX);
}

void ED_region_do_msg_notify_tag_redraw(
    /* Follow wmMsgNotifyFn spec */
    bContext *UNUSED(C),
    wmMsgSubscribeKey *UNUSED(msg_key),
    wmMsgSubscribeValue *msg_val)
{
  ARegion *region = msg_val->owner;
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
    bContext *UNUSED(C),
    wmMsgSubscribeKey *UNUSED(msg_key),
    wmMsgSubscribeValue *msg_val)
{
  ScrArea *area = msg_val->user_data;
  ED_area_tag_refresh(area);
}

void ED_area_do_mgs_subscribe_for_tool_header(
    /* Follow ARegionType.message_subscribe */
    const struct bContext *UNUSED(C),
    struct WorkSpace *workspace,
    struct Scene *UNUSED(scene),
    struct bScreen *UNUSED(screen),
    struct ScrArea *UNUSED(area),
    struct ARegion *region,
    struct wmMsgBus *mbus)
{
  BLI_assert(region->regiontype == RGN_TYPE_TOOL_HEADER);
  wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
      .owner = region,
      .user_data = region,
      .notify = ED_region_do_msg_notify_tag_redraw,
  };
  WM_msg_subscribe_rna_prop(
      mbus, &workspace->id, workspace, WorkSpace, tools, &msg_sub_value_region_tag_redraw);
}

void ED_area_do_mgs_subscribe_for_tool_ui(
    /* Follow ARegionType.message_subscribe */
    const struct bContext *UNUSED(C),
    struct WorkSpace *workspace,
    struct Scene *UNUSED(scene),
    struct bScreen *UNUSED(screen),
    struct ScrArea *UNUSED(area),
    struct ARegion *region,
    struct wmMsgBus *mbus)
{
  BLI_assert(region->regiontype == RGN_TYPE_UI);
  const char *category = UI_panel_category_active_get(region, false);
  if (category && STREQ(category, "Tool")) {
    wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
        .owner = region,
        .user_data = region,
        .notify = ED_region_do_msg_notify_tag_redraw,
    };
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

/* only exported for WM */
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
}

/* only exported for WM */
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
    glClear(GL_COLOR_BUFFER_BIT);
    return;
  }
  /* optional header info instead? */
  else if (region->headerstr) {
    region_draw_status_text(area, region);
  }
  else if (at->draw) {
    at->draw(C, region);
  }

  /* XXX test: add convention to end regions always in pixel space,
   * for drawing of borders/gestures etc */
  ED_region_pixelspace(region);

  ED_region_draw_cb_draw(C, region, REGION_DRAW_POST_PIXEL);

  region_draw_azones(area, region);

  /* for debugging unneeded area redraws and partial redraw */
  if (G.debug_value == 888) {
    GPU_blend(true);
    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformColor4f(BLI_thread_frand(0), BLI_thread_frand(0), BLI_thread_frand(0), 0.1f);
    immRectf(pos,
             region->drawrct.xmin - region->winrct.xmin,
             region->drawrct.ymin - region->winrct.ymin,
             region->drawrct.xmax - region->winrct.xmin,
             region->drawrct.ymax - region->winrct.ymin);
    immUnbindProgram();
    GPU_blend(false);
  }

  memset(&region->drawrct, 0, sizeof(region->drawrct));

  UI_blocklist_free_inactive(C, &region->uiblocks);

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
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
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
    struct wmMsgBus *mbus = wm->message_bus;
    WM_msgbus_clear_by_owner(mbus, region);

    /* Cheat, always subscribe to this space type properties.
     *
     * This covers most cases and avoids copy-paste similar code for each space type.
     */
    if (ELEM(
            region->regiontype, RGN_TYPE_WINDOW, RGN_TYPE_CHANNELS, RGN_TYPE_UI, RGN_TYPE_TOOLS)) {
      SpaceLink *sl = area->spacedata.first;

      PointerRNA ptr;
      RNA_pointer_create(&screen->id, &RNA_Space, sl, &ptr);

      wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
          .owner = region,
          .user_data = region,
          .notify = ED_region_do_msg_notify_tag_redraw,
      };
      /* All properties for this space type. */
      WM_msg_subscribe_rna(mbus, &ptr, NULL, &msg_sub_value_region_tag_redraw, __func__);
    }

    ED_region_message_subscribe(C, workspace, scene, screen, area, region, mbus);
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
    region->do_draw_overlay = RGN_DRAW;
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

/**
 * Tag editor overlays to be redrawn. If in doubt about which parts need to be redrawn (partial
 * clipping rectangle set), redraw everything.
 */
void ED_region_tag_redraw_editor_overlays(struct ARegion *region)
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
  ARegion *region;

  if (area) {
    for (region = area->regionbase.first; region; region = region->next) {
      ED_region_tag_redraw(region);
    }
  }
}

void ED_area_tag_redraw_no_rebuild(ScrArea *area)
{
  ARegion *region;

  if (area) {
    for (region = area->regionbase.first; region; region = region->next) {
      ED_region_tag_redraw_no_rebuild(region);
    }
  }
}

void ED_area_tag_redraw_regiontype(ScrArea *area, int regiontype)
{
  ARegion *region;

  if (area) {
    for (region = area->regionbase.first; region; region = region->next) {
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

/* *************************************************************** */

/* use NULL to disable it */
void ED_area_status_text(ScrArea *area, const char *str)
{
  ARegion *region;

  /* happens when running transform operators in background mode */
  if (area == NULL) {
    return;
  }

  for (region = area->regionbase.first; region; region = region->next) {
    if (region->regiontype == RGN_TYPE_HEADER) {
      if (str) {
        if (region->headerstr == NULL) {
          region->headerstr = MEM_mallocN(UI_MAX_DRAW_STR, "headerprint");
        }
        BLI_strncpy(region->headerstr, str, UI_MAX_DRAW_STR);
        BLI_str_rstrip(region->headerstr);
      }
      else if (region->headerstr) {
        MEM_freeN(region->headerstr);
        region->headerstr = NULL;
      }
      ED_region_tag_redraw(region);
    }
  }
}

void ED_workspace_status_text(bContext *C, const char *str)
{
  wmWindow *win = CTX_wm_window(C);
  WorkSpace *workspace = CTX_wm_workspace(C);

  /* Can be NULL when running operators in background mode. */
  if (workspace == NULL) {
    return;
  }

  if (str) {
    if (workspace->status_text == NULL) {
      workspace->status_text = MEM_mallocN(UI_MAX_DRAW_STR, "headerprint");
    }
    BLI_strncpy(workspace->status_text, str, UI_MAX_DRAW_STR);
  }
  else if (workspace->status_text) {
    MEM_freeN(workspace->status_text);
    workspace->status_text = NULL;
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

static void area_azone_initialize(wmWindow *win, const bScreen *screen, ScrArea *area)
{
  AZone *az;

  /* reinitialize entirely, regions and fullscreen add azones too */
  BLI_freelistN(&area->actionzones);

  if (screen->state != SCREENNORMAL) {
    return;
  }

  if (U.app_flag & USER_APP_LOCK_UI_LAYOUT) {
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
         (coords[i][0] == WM_window_pixels_x(win) && coords[i][1] == 0))) {
      continue;
    }
#else
    (void)win;
#endif

    /* set area action zones */
    az = (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
    BLI_addtail(&(area->actionzones), az);
    az->type = AZONE_AREA;
    az->x1 = coords[i][0];
    az->y1 = coords[i][1];
    az->x2 = coords[i][2];
    az->y2 = coords[i][3];
    BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
  }
}

static void fullscreen_azone_initialize(ScrArea *area, ARegion *region)
{
  AZone *az;

  if (ED_area_is_global(area) || (region->regiontype != RGN_TYPE_WINDOW)) {
    return;
  }

  az = (AZone *)MEM_callocN(sizeof(AZone), "fullscreen action zone");
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

#define AZONEPAD_EDGE (0.1f * U.widget_unit)
#define AZONEPAD_ICON (0.45f * U.widget_unit)
static void region_azone_edge(AZone *az, ARegion *region)
{
  switch (az->edge) {
    case AE_TOP_TO_BOTTOMRIGHT:
      az->x1 = region->winrct.xmin;
      az->y1 = region->winrct.ymax - AZONEPAD_EDGE;
      az->x2 = region->winrct.xmax;
      az->y2 = region->winrct.ymax + AZONEPAD_EDGE;
      break;
    case AE_BOTTOM_TO_TOPLEFT:
      az->x1 = region->winrct.xmin;
      az->y1 = region->winrct.ymin + AZONEPAD_EDGE;
      az->x2 = region->winrct.xmax;
      az->y2 = region->winrct.ymin - AZONEPAD_EDGE;
      break;
    case AE_LEFT_TO_TOPRIGHT:
      az->x1 = region->winrct.xmin - AZONEPAD_EDGE;
      az->y1 = region->winrct.ymin;
      az->x2 = region->winrct.xmin + AZONEPAD_EDGE;
      az->y2 = region->winrct.ymax;
      break;
    case AE_RIGHT_TO_TOPLEFT:
      az->x1 = region->winrct.xmax + AZONEPAD_EDGE;
      az->y1 = region->winrct.ymin;
      az->x2 = region->winrct.xmax - AZONEPAD_EDGE;
      az->y2 = region->winrct.ymax;
      break;
  }
  BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
}

/* region already made zero sized, in shape of edge */
static void region_azone_tab_plus(ScrArea *area, AZone *az, ARegion *region)
{
  AZone *azt;
  int tot = 0, add;
  /* Edge offset multiplied by the  */

  float edge_offset = 1.0f;
  const float tab_size_x = 0.7f * U.widget_unit;
  const float tab_size_y = 0.4f * U.widget_unit;

  for (azt = area->actionzones.first; azt; azt = azt->next) {
    if (azt->edge == az->edge) {
      tot++;
    }
  }

  switch (az->edge) {
    case AE_TOP_TO_BOTTOMRIGHT:
      add = (region->winrct.ymax == area->totrct.ymin) ? 1 : 0;
      az->x1 = region->winrct.xmax - ((edge_offset + 1.0f) * tab_size_x);
      az->y1 = region->winrct.ymax - add;
      az->x2 = region->winrct.xmax - (edge_offset * tab_size_x);
      az->y2 = region->winrct.ymax - add + tab_size_y;
      break;
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
  const bool is_hidden = (region->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL));

  if (is_hidden && is_fullscreen) {
    return false;
  }
  if (!is_hidden && ELEM(region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
    return false;
  }

  return true;
}

static void region_azone_edge_initialize(ScrArea *area,
                                         ARegion *region,
                                         AZEdge edge,
                                         const bool is_fullscreen)
{
  AZone *az = NULL;
  const bool is_hidden = (region->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL));

  if (!region_azone_edge_poll(region, is_fullscreen)) {
    return;
  }

  az = (AZone *)MEM_callocN(sizeof(AZone), "actionzone");
  BLI_addtail(&(area->actionzones), az);
  az->type = AZONE_REGION;
  az->region = region;
  az->edge = edge;

  if (is_hidden) {
    region_azone_tab_plus(area, az, region);
  }
  else {
    region_azone_edge(az, region);
  }
}

static void region_azone_scrollbar_initialize(ScrArea *area,
                                              ARegion *region,
                                              AZScrollDirection direction)
{
  rcti scroller_vert = (direction == AZ_SCROLL_VERT) ? region->v2d.vert : region->v2d.hor;
  AZone *az = MEM_callocN(sizeof(*az), __func__);

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

  BLI_rcti_translate(&scroller_vert, region->winrct.xmin, region->winrct.ymin);
  az->x1 = scroller_vert.xmin - AZONEFADEIN;
  az->y1 = scroller_vert.ymin - AZONEFADEIN;
  az->x2 = scroller_vert.xmax + AZONEFADEIN;
  az->y2 = scroller_vert.ymax + AZONEFADEIN;

  BLI_rcti_init(&az->rect, az->x1, az->x2, az->y1, az->y2);
}

static void region_azones_scrollbars_initialize(ScrArea *area, ARegion *region)
{
  const View2D *v2d = &region->v2d;

  if ((v2d->scroll & V2D_SCROLL_VERTICAL) && ((v2d->scroll & V2D_SCROLL_VERTICAL_HANDLES) == 0)) {
    region_azone_scrollbar_initialize(area, region, AZ_SCROLL_VERT);
  }
  if ((v2d->scroll & V2D_SCROLL_HORIZONTAL) &&
      ((v2d->scroll & V2D_SCROLL_HORIZONTAL_HANDLES) == 0)) {
    region_azone_scrollbar_initialize(area, region, AZ_SCROLL_HOR);
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
    region_azone_edge_initialize(area, region, AE_BOTTOM_TO_TOPLEFT, is_fullscreen);
  }
  else if (alignment == RGN_ALIGN_BOTTOM) {
    region_azone_edge_initialize(area, region, AE_TOP_TO_BOTTOMRIGHT, is_fullscreen);
  }
  else if (alignment == RGN_ALIGN_RIGHT) {
    region_azone_edge_initialize(area, region, AE_LEFT_TO_TOPRIGHT, is_fullscreen);
  }
  else if (alignment == RGN_ALIGN_LEFT) {
    region_azone_edge_initialize(area, region, AE_RIGHT_TO_TOPLEFT, is_fullscreen);
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
    fullscreen_azone_initialize(area, region);
  }

  region_azones_scrollbars_initialize(area, region);
}

/* dir is direction to check, not the splitting edge direction! */
static int rct_fits(const rcti *rect, char dir, int size)
{
  if (dir == 'h') {
    return BLI_rcti_size_x(rect) + 1 - size;
  }
  else { /* 'v' */
    return BLI_rcti_size_y(rect) + 1 - size;
  }
}

/* *************************************************************** */

/* region should be overlapping */
/* function checks if some overlapping region was defined before - on same place */
static void region_overlap_fix(ScrArea *area, ARegion *region)
{
  ARegion *ar1;
  const int align = RGN_ALIGN_ENUM_FROM_MASK(region->alignment);
  int align1 = 0;

  /* find overlapping previous region on same place */
  for (ar1 = region->prev; ar1; ar1 = ar1->prev) {
    if (ar1->flag & RGN_FLAG_HIDDEN) {
      continue;
    }

    if (ar1->overlap && ((ar1->alignment & RGN_SPLIT_PREV) == 0)) {
      if (ELEM(ar1->alignment, RGN_ALIGN_FLOAT)) {
        continue;
      }
      align1 = ar1->alignment;
      if (BLI_rcti_isect(&ar1->winrct, &region->winrct, NULL)) {
        if (align1 != align) {
          /* Left overlapping right or vice-versa, forbid this! */
          region->flag |= RGN_FLAG_TOO_SMALL;
          return;
        }
        /* Else, we have our previous region on same side. */
        break;
      }
    }
  }

  /* Guard against flags slipping through that would have to be masked out in usages below. */
  BLI_assert(align1 == RGN_ALIGN_ENUM_FROM_MASK(align1));

  /* translate or close */
  if (ar1) {
    if (align1 == RGN_ALIGN_LEFT) {
      if (region->winrct.xmax + ar1->winx > area->winx - U.widget_unit) {
        region->flag |= RGN_FLAG_TOO_SMALL;
        return;
      }
      else {
        BLI_rcti_translate(&region->winrct, ar1->winx, 0);
      }
    }
    else if (align1 == RGN_ALIGN_RIGHT) {
      if (region->winrct.xmin - ar1->winx < U.widget_unit) {
        region->flag |= RGN_FLAG_TOO_SMALL;
        return;
      }
      else {
        BLI_rcti_translate(&region->winrct, -ar1->winx, 0);
      }
    }
  }

  /* At this point, 'region' is in its final position and still open.
   * Make a final check it does not overlap any previous 'other side' region. */
  for (ar1 = region->prev; ar1; ar1 = ar1->prev) {
    if (ar1->flag & RGN_FLAG_HIDDEN) {
      continue;
    }
    if (ELEM(ar1->alignment, RGN_ALIGN_FLOAT)) {
      continue;
    }

    if (ar1->overlap && (ar1->alignment & RGN_SPLIT_PREV) == 0) {
      if ((ar1->alignment != align) && BLI_rcti_isect(&ar1->winrct, &region->winrct, NULL)) {
        /* Left overlapping right or vice-versa, forbid this! */
        region->flag |= RGN_FLAG_TOO_SMALL;
        return;
      }
    }
  }
}

/* overlapping regions only in the following restricted cases */
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
    else if (ELEM(spacetype, SPACE_VIEW3D, SPACE_IMAGE)) {
      if (ELEM(regiontype,
               RGN_TYPE_TOOLS,
               RGN_TYPE_UI,
               RGN_TYPE_TOOL_PROPS,
               RGN_TYPE_HEADER,
               RGN_TYPE_FOOTER)) {
        return true;
      }
    }
  }

  return false;
}

static void region_rect_recursive(
    ScrArea *area, ARegion *region, rcti *remainder, rcti *overlap_remainder, int quad)
{
  rcti *remainder_prev = remainder;

  if (region == NULL) {
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
  if ((region->next == NULL) && !ELEM(alignment, RGN_ALIGN_QSPLIT, RGN_ALIGN_FLOAT)) {
    alignment = RGN_ALIGN_NONE;
  }

  /* If both the ARegion.sizex/y and the prefsize are 0, the region is tagged as too small, even
   * before the layout for dynamic regions is created. #wm_draw_window_offscreen() allows the
   * layout to be created despite the RGN_FLAG_TOO_SMALL flag being set. But there may still be
   * regions that don't have a separate ARegionType.layout callback. For those, set a default
   * prefsize so they can become visible. */
  if ((region->flag & RGN_FLAG_DYNAMIC_SIZE) && !(region->type->layout)) {
    if ((region->sizex == 0) && (region->type->prefsizex == 0)) {
      region->type->prefsizex = AREAMINX;
    }
    if ((region->sizey == 0) && (region->type->prefsizey == 0)) {
      region->type->prefsizey = HEADERY;
    }
  }

  /* prefsize, taking into account DPI */
  int prefsizex = UI_DPI_FAC *
                  ((region->sizex > 1) ? region->sizex + 0.5f : region->type->prefsizex);
  int prefsizey;

  if (region->flag & RGN_FLAG_PREFSIZE_OR_HIDDEN) {
    prefsizex = UI_DPI_FAC * region->type->prefsizex;
    prefsizey = UI_DPI_FAC * region->type->prefsizey;
  }
  else if (region->regiontype == RGN_TYPE_HEADER) {
    prefsizey = ED_area_headersize();
  }
  else if (region->regiontype == RGN_TYPE_TOOL_HEADER) {
    prefsizey = ED_area_headersize();
  }
  else if (region->regiontype == RGN_TYPE_FOOTER) {
    prefsizey = ED_area_footersize();
  }
  else if (ED_area_is_global(area)) {
    prefsizey = ED_region_global_size_y();
  }
  else {
    prefsizey = UI_DPI_FAC * (region->sizey > 1 ? region->sizey + 0.5f : region->type->prefsizey);
  }

  if (region->flag & RGN_FLAG_HIDDEN) {
    /* hidden is user flag */
  }
  else if (alignment == RGN_ALIGN_FLOAT) {
    /**
     * \note Currently this window type is only used for #RGN_TYPE_HUD,
     * We expect the panel to resize it's self to be larger.
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

    /* We need to use a test that wont have been previously clamped. */
    rcti winrct_test = {
        .xmin = region->winrct.xmin,
        .ymin = region->winrct.ymin,
        .xmax = region->winrct.xmin + size_min[0],
        .ymax = region->winrct.ymin + size_min[1],
    };
    BLI_rcti_isect(&winrct_test, &overlap_remainder_margin, &winrct_test);
    if (BLI_rcti_size_x(&winrct_test) < size_min[0] ||
        BLI_rcti_size_y(&winrct_test) < size_min[1]) {
      region->flag |= RGN_FLAG_TOO_SMALL;
    }
  }
  else if (rct_fits(remainder, 'v', 1) < 0 || rct_fits(remainder, 'h', 1) < 0) {
    /* remainder is too small for any usage */
    region->flag |= RGN_FLAG_TOO_SMALL;
  }
  else if (alignment == RGN_ALIGN_NONE) {
    /* typically last region */
    region->winrct = *remainder;
    BLI_rcti_init(remainder, 0, 0, 0, 0);
  }
  else if (alignment == RGN_ALIGN_TOP || alignment == RGN_ALIGN_BOTTOM) {
    rcti *winrct = (region->overlap) ? overlap_remainder : remainder;

    if ((prefsizey == 0) || (rct_fits(winrct, 'v', prefsizey) < 0)) {
      region->flag |= RGN_FLAG_TOO_SMALL;
    }
    else {
      int fac = rct_fits(winrct, 'v', prefsizey);

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

    if ((prefsizex == 0) || (rct_fits(winrct, 'h', prefsizex) < 0)) {
      region->flag |= RGN_FLAG_TOO_SMALL;
    }
    else {
      int fac = rct_fits(winrct, 'h', prefsizex);

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
  else if (alignment == RGN_ALIGN_VSPLIT || alignment == RGN_ALIGN_HSPLIT) {
    /* percentage subdiv*/
    region->winrct = *remainder;

    if (alignment == RGN_ALIGN_HSPLIT) {
      if (rct_fits(remainder, 'h', prefsizex) > 4) {
        region->winrct.xmax = BLI_rcti_cent_x(remainder);
        remainder->xmin = region->winrct.xmax + 1;
      }
      else {
        BLI_rcti_init(remainder, 0, 0, 0, 0);
      }
    }
    else {
      if (rct_fits(remainder, 'v', prefsizey) > 4) {
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
      ARegion *artest = region->next;
      int count = 1;

      while (artest) {
        artest->alignment = RGN_ALIGN_QSPLIT;
        artest = artest->next;
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

      /* Fix any negative dimensions. This can happen when a quad split 3d view gets to small. (see
       * T72200). */
      BLI_rcti_sanitize(&region->winrct);

      quad++;
    }
  }

  /* for speedup */
  region->winx = BLI_rcti_size_x(&region->winrct) + 1;
  region->winy = BLI_rcti_size_y(&region->winrct) + 1;

  /* if region opened normally, we store this for hide/reveal usage */
  /* prevent rounding errors for UI_DPI_FAC mult and divide */
  if (region->winx > 1) {
    region->sizex = (region->winx + 0.5f) / UI_DPI_FAC;
  }
  if (region->winy > 1) {
    region->sizey = (region->winy + 0.5f) / UI_DPI_FAC;
  }

  /* exception for multiple overlapping regions on same spot */
  if (region->overlap && (alignment != RGN_ALIGN_FLOAT)) {
    region_overlap_fix(area, region);
  }

  /* set winrect for azones */
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
  short px = (short)U.pixelsize;

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
   * For more details see T54864. */
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

/* used for area initialize below */
static void region_subwindow(ARegion *region)
{
  bool hidden = (region->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL)) != 0;

  if ((region->alignment & RGN_SPLIT_PREV) && region->prev) {
    hidden = hidden || (region->prev->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_TOO_SMALL));
  }

  region->visible = !hidden;
}

static bool event_in_markers_region(const ARegion *region, const wmEvent *event)
{
  rcti rect = region->winrct;
  rect.ymax = rect.ymin + UI_MARKER_MARGIN_Y;
  return BLI_rcti_isect_pt(&rect, event->x, event->y);
}

/**
 * \param region: Region, may be NULL when adding handlers for \a area.
 */
static void ed_default_handlers(
    wmWindowManager *wm, ScrArea *area, ARegion *region, ListBase *handlers, int flag)
{
  BLI_assert(region ? (&region->handlers == handlers) : (&area->handlers == handlers));

  /* note, add-handler checks if it already exists */

  /* XXX it would be good to have boundbox checks for some of these... */
  if (flag & ED_KEYMAP_UI) {
    wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "User Interface", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap);

    /* user interface widgets */
    UI_region_handlers_add(handlers);
  }
  if (flag & ED_KEYMAP_GIZMO) {
    BLI_assert(region && ELEM(region->type->regionid, RGN_TYPE_WINDOW, RGN_TYPE_PREVIEW));
    if (region) {
      /* Anything else is confusing, only allow this. */
      BLI_assert(&region->handlers == handlers);
      if (region->gizmo_map == NULL) {
        region->gizmo_map = WM_gizmomap_new_from_type(
            &(const struct wmGizmoMapType_Params){area->spacetype, region->type->regionid});
      }
      WM_gizmomap_add_handlers(region, region->gizmo_map);
    }
  }
  if (flag & ED_KEYMAP_VIEW2D) {
    /* 2d-viewport handling+manipulation */
    wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "View2D", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap);
  }
  if (flag & ED_KEYMAP_ANIMATION) {
    wmKeyMap *keymap;

    /* time-markers */
    keymap = WM_keymap_ensure(wm->defaultconf, "Markers", 0, 0);
    WM_event_add_keymap_handler_poll(handlers, keymap, event_in_markers_region);

    /* time-scrub */
    keymap = WM_keymap_ensure(wm->defaultconf, "Time Scrub", 0, 0);
    WM_event_add_keymap_handler_poll(handlers, keymap, ED_time_scrub_event_in_region);

    /* frame changing and timeline operators (for time spaces) */
    keymap = WM_keymap_ensure(wm->defaultconf, "Animation", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap);
  }
  if (flag & ED_KEYMAP_TOOL) {
    WM_event_add_keymap_handler_dynamic(
        &region->handlers, WM_event_get_keymap_from_toolsystem_fallback, area);
    WM_event_add_keymap_handler_dynamic(
        &region->handlers, WM_event_get_keymap_from_toolsystem, area);
  }
  if (flag & ED_KEYMAP_FRAMES) {
    /* frame changing/jumping (for all spaces) */
    wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "Frames", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap);
  }
  if (flag & ED_KEYMAP_HEADER) {
    /* standard keymap for headers regions */
    wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "Region Context Menu", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap);
  }
  if (flag & ED_KEYMAP_FOOTER) {
    /* standard keymap for footer regions */
    wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "Region Context Menu", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap);
  }
  if (flag & ED_KEYMAP_NAVBAR) {
    /* standard keymap for Navigation bar regions */
    wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "Region Context Menu", 0, 0);
    WM_event_add_keymap_handler(&region->handlers, keymap);
  }

  /* Keep last because of LMB/RMB handling, see: T57527. */
  if (flag & ED_KEYMAP_GPENCIL) {
    /* grease pencil */
    /* NOTE: This is now 4 keymaps - One for basic functionality,
     *       and others for special stroke modes (edit, paint and sculpt).
     *
     *       For now, it's easier to just include all,
     *       since you hardly want one without the others.
     */
    wmKeyMap *keymap_general = WM_keymap_ensure(wm->defaultconf, "Grease Pencil", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_general);

    wmKeyMap *keymap_edit = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Edit Mode", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_edit);

    wmKeyMap *keymap_paint = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Paint Mode", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_paint);

    wmKeyMap *keymap_paint_draw = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Paint (Draw brush)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_paint_draw);

    wmKeyMap *keymap_paint_erase = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Paint (Erase)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_paint_erase);

    wmKeyMap *keymap_paint_fill = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Paint (Fill)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_paint_fill);

    wmKeyMap *keymap_paint_tint = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Paint (Tint)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_paint_tint);

    wmKeyMap *keymap_sculpt = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Sculpt Mode", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_sculpt);

    wmKeyMap *keymap_vertex = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Vertex Mode", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_vertex);

    wmKeyMap *keymap_vertex_draw = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Vertex (Draw)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_vertex_draw);

    wmKeyMap *keymap_vertex_blur = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Vertex (Blur)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_vertex_blur);

    wmKeyMap *keymap_vertex_average = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Vertex (Average)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_vertex_average);

    wmKeyMap *keymap_vertex_smear = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Vertex (Smear)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_vertex_smear);

    wmKeyMap *keymap_vertex_replace = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Vertex (Replace)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_vertex_replace);

    wmKeyMap *keymap_sculpt_smooth = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Sculpt (Smooth)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_sculpt_smooth);

    wmKeyMap *keymap_sculpt_thickness = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Sculpt (Thickness)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_sculpt_thickness);

    wmKeyMap *keymap_sculpt_strength = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Sculpt (Strength)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_sculpt_strength);

    wmKeyMap *keymap_sculpt_grab = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Sculpt (Grab)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_sculpt_grab);

    wmKeyMap *keymap_sculpt_push = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Sculpt (Push)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_sculpt_push);

    wmKeyMap *keymap_sculpt_twist = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Sculpt (Twist)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_sculpt_twist);

    wmKeyMap *keymap_sculpt_pinch = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Sculpt (Pinch)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_sculpt_pinch);

    wmKeyMap *keymap_sculpt_randomize = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Sculpt (Randomize)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_sculpt_randomize);

    wmKeyMap *keymap_sculpt_clone = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Sculpt (Clone)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_sculpt_clone);

    wmKeyMap *keymap_weight = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Weight Mode", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_weight);

    wmKeyMap *keymap_weight_draw = WM_keymap_ensure(
        wm->defaultconf, "Grease Pencil Stroke Weight (Draw)", 0, 0);
    WM_event_add_keymap_handler(handlers, keymap_weight_draw);
  }
}

void ED_area_update_region_sizes(wmWindowManager *wm, wmWindow *win, ScrArea *area)
{
  rcti rect, overlap_rect;
  rcti window_rect;

  if (!(area->flag & AREA_FLAG_REGION_SIZE_UPDATE)) {
    return;
  }
  const bScreen *screen = WM_window_get_active_screen(win);

  WM_window_rect_calc(win, &window_rect);
  area_calc_totrct(area, &window_rect);

  /* region rect sizes */
  rect = area->totrct;
  overlap_rect = rect;
  region_rect_recursive(area, area->regionbase.first, &rect, &overlap_rect, 0);

  /* Dynamically sized regions may have changed region sizes, so we have to force azone update. */
  area_azone_initialize(win, screen, area);

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    region_subwindow(region);

    /* region size may have changed, init does necessary adjustments */
    if (region->type->init) {
      region->type->init(wm, region);
    }

    /* Some AZones use View2D data which is only updated in region init, so call that first! */
    region_azones_add(screen, area, region);
  }
  ED_area_azones_update(area, &win->eventstate->x);

  area->flag &= ~AREA_FLAG_REGION_SIZE_UPDATE;
}

/* called in screen_refresh, or screens_init, also area size changes */
void ED_area_initialize(wmWindowManager *wm, wmWindow *win, ScrArea *area)
{
  WorkSpace *workspace = WM_window_get_active_workspace(win);
  const bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);
  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  ARegion *region;
  rcti rect, overlap_rect;
  rcti window_rect;

  if (ED_area_is_global(area) && (area->global->flag & GLOBAL_AREA_IS_HIDDEN)) {
    return;
  }
  WM_window_rect_calc(win, &window_rect);

  /* set typedefinitions */
  area->type = BKE_spacetype_from_id(area->spacetype);

  if (area->type == NULL) {
    area->spacetype = SPACE_VIEW3D;
    area->type = BKE_spacetype_from_id(area->spacetype);
  }

  for (region = area->regionbase.first; region; region = region->next) {
    region->type = BKE_regiontype_from_id_or_first(area->type, region->regiontype);
  }

  /* area sizes */
  area_calc_totrct(area, &window_rect);

  /* region rect sizes */
  rect = area->totrct;
  overlap_rect = rect;
  region_rect_recursive(area, area->regionbase.first, &rect, &overlap_rect, 0);
  area->flag &= ~AREA_FLAG_REGION_SIZE_UPDATE;

  /* default area handlers */
  ed_default_handlers(wm, area, NULL, &area->handlers, area->type->keymapflag);
  /* checks spacedata, adds own handlers */
  if (area->type->init) {
    area->type->init(wm, area);
  }

  /* clear all azones, add the area triangle widgets */
  area_azone_initialize(win, screen, area);

  /* region windows, default and own handlers */
  for (region = area->regionbase.first; region; region = region->next) {
    region_subwindow(region);

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
      UI_blocklist_free(NULL, &region->uiblocks);
    }

    /* Some AZones use View2D data which is only updated in region init, so call that first! */
    region_azones_add(screen, area, region);
  }

  /* Avoid re-initializing tools while resizing the window. */
  if ((G.moving & G_TRANSFORM_WM) == 0) {
    if ((1 << area->spacetype) & WM_TOOLSYSTEM_SPACE_MASK) {
      WM_toolsystem_refresh_screen_area(workspace, view_layer, area);
      area->flag |= AREA_FLAG_ACTIVE_TOOL_UPDATE;
    }
    else {
      area->runtime.tool = NULL;
      area->runtime.is_tool_set = true;
    }
  }
}

static void region_update_rect(ARegion *region)
{
  region->winx = BLI_rcti_size_x(&region->winrct) + 1;
  region->winy = BLI_rcti_size_y(&region->winrct) + 1;

  /* v2d mask is used to subtract scrollbars from a 2d view. Needs initialize here. */
  BLI_rcti_init(&region->v2d.mask, 0, region->winx - 1, 0, region->winy - 1);
}

/**
 * Call to move a popup window (keep OpenGL context free!)
 */
void ED_region_update_rect(ARegion *region)
{
  region_update_rect(region);
}

/* externally called for floating regions like menus */
void ED_region_floating_initialize(ARegion *region)
{
  BLI_assert(region->alignment == RGN_ALIGN_FLOAT);

  /* refresh can be called before window opened */
  region_subwindow(region);

  region_update_rect(region);
}

void ED_region_cursor_set(wmWindow *win, ScrArea *area, ARegion *region)
{
  if (region != NULL) {
    if ((region->gizmo_map != NULL) && WM_gizmomap_cursor_set(region->gizmo_map, win)) {
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

/* for use after changing visibility of regions */
void ED_region_visibility_change_update(bContext *C, ScrArea *area, ARegion *region)
{
  if (region->flag & RGN_FLAG_HIDDEN) {
    WM_event_remove_handlers(C, &region->handlers);
  }

  ED_area_initialize(CTX_wm_manager(C), CTX_wm_window(C), area);
  ED_area_tag_redraw(area);
}

/* for quick toggle, can skip fades */
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

/* exported to all editors, uses fading default */
void ED_region_toggle_hidden(bContext *C, ARegion *region)
{
  region_toggle_hidden(C, region, true);
}

/**
 * we swap spaces for fullscreen to keep all allocated data area vertices were set
 */
void ED_area_data_copy(ScrArea *area_dst, ScrArea *area_src, const bool do_free)
{
  SpaceType *st;
  ARegion *region;
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

  /* Note; SPACE_EMPTY is possible on new screens */

  /* regions */
  if (do_free) {
    st = BKE_spacetype_from_id(spacetype);
    for (region = area_dst->regionbase.first; region; region = region->next) {
      BKE_area_region_free(st, region);
    }
    BLI_freelistN(&area_dst->regionbase);
  }
  st = BKE_spacetype_from_id(area_src->spacetype);
  for (region = area_src->regionbase.first; region; region = region->next) {
    ARegion *newar = BKE_area_region_copy(st, region);
    BLI_addtail(&area_dst->regionbase, newar);
  }
}

void ED_area_data_swap(ScrArea *area_dst, ScrArea *area_src)
{
  SWAP(char, area_dst->spacetype, area_src->spacetype);
  SWAP(SpaceType *, area_dst->type, area_src->type);

  SWAP(ListBase, area_dst->spacedata, area_src->spacedata);
  SWAP(ListBase, area_dst->regionbase, area_src->regionbase);
}

/* *********** Space switching code *********** */

void ED_area_swapspace(bContext *C, ScrArea *sa1, ScrArea *sa2)
{
  ScrArea *tmp = MEM_callocN(sizeof(ScrArea), "addscrarea");
  wmWindow *win = CTX_wm_window(C);

  ED_area_exit(C, sa1);
  ED_area_exit(C, sa2);

  ED_area_data_copy(tmp, sa1, false);
  ED_area_data_copy(sa1, sa2, true);
  ED_area_data_copy(sa2, tmp, true);
  ED_area_initialize(CTX_wm_manager(C), win, sa1);
  ED_area_initialize(CTX_wm_manager(C), win, sa2);

  BKE_screen_area_free(tmp);
  MEM_freeN(tmp);

  /* tell WM to refresh, cursor types etc */
  WM_event_add_mousemove(win);

  ED_area_tag_redraw(sa1);
  ED_area_tag_refresh(sa1);
  ED_area_tag_redraw(sa2);
  ED_area_tag_refresh(sa2);
}

/**
 * \param skip_region_exit: Skip calling area exit callback. Set for opening temp spaces.
 */
void ED_area_newspace(bContext *C, ScrArea *area, int type, const bool skip_region_exit)
{
  wmWindow *win = CTX_wm_window(C);

  if (area->spacetype != type) {
    SpaceType *st;
    SpaceLink *slold = area->spacedata.first;
    SpaceLink *sl;
    /* store area->type->exit callback */
    void *area_exit = area->type ? area->type->exit : NULL;
    /* When the user switches between space-types from the type-selector,
     * changing the header-type is jarring (especially when using Ctrl-MouseWheel).
     *
     * However, add-on install for example, forces the header to the top which shouldn't
     * be applied back to the previous space type when closing - see: T57724
     *
     * Newly created windows wont have any space data, use the alignment
     * the space type defaults to in this case instead
     * (needed for preferences to have space-type on bottom).
     */
    int header_alignment = ED_area_header_alignment_or_fallback(area, -1);
    const bool sync_header_alignment = ((header_alignment != -1) &&
                                        ((slold->link_flag & SPACE_FLAG_TYPE_TEMPORARY) == 0));

    /* in some cases (opening temp space) we don't want to
     * call area exit callback, so we temporarily unset it */
    if (skip_region_exit && area->type) {
      area->type->exit = NULL;
    }

    ED_area_exit(C, area);

    /* restore old area exit callback */
    if (skip_region_exit && area->type) {
      area->type->exit = area_exit;
    }

    st = BKE_spacetype_from_id(type);

    area->spacetype = type;
    area->type = st;

    /* If st->new may be called, don't use context until then. The
     * area->type->context() callback has changed but data may be invalid
     * (e.g. with properties editor) until space-data is properly created */

    /* check previously stored space */
    for (sl = area->spacedata.first; sl; sl = sl->next) {
      if (sl->spacetype == type) {
        break;
      }
    }

    /* old spacedata... happened during work on 2.50, remove */
    if (sl && BLI_listbase_is_empty(&sl->regionbase)) {
      st->free(sl);
      BLI_freelinkN(&area->spacedata, sl);
      if (slold == sl) {
        slold = NULL;
      }
      sl = NULL;
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
        sl = st->new (area, scene);
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
      /* Spaces with footer. */
      if (st->spaceid == SPACE_TEXT) {
        LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
          if (ELEM(region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
            region->alignment = header_alignment;
          }
          if (region->regiontype == RGN_TYPE_FOOTER) {
            int footer_alignment = (header_alignment == RGN_ALIGN_BOTTOM) ? RGN_ALIGN_TOP :
                                                                            RGN_ALIGN_BOTTOM;
            region->alignment = footer_alignment;
            break;
          }
        }
      }
      else {
        LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
          if (ELEM(region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
            region->alignment = header_alignment;
            break;
          }
        }
      }
    }

    ED_area_initialize(CTX_wm_manager(C), win, area);

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
  SpaceLink *sl = area->spacedata.first;

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
  SpaceLink *sl = area->spacedata.first;
  SpaceLink *prevspace = sl ? area_get_prevspace(area) : NULL;

  if (prevspace) {
    ED_area_newspace(C, area, prevspace->spacetype, false);
    /* We've exited the space, so it can't be considered temporary anymore. */
    sl->link_flag &= ~SPACE_FLAG_TYPE_TEMPORARY;
  }
  else {
    /* no change */
    return;
  }
  /* If this is a stacked fullscreen, changing to previous area exits it (meaning we're still in a
   * fullscreen, but not in a stacked one). */
  area->flag &= ~AREA_FLAG_STACKED_FULLSCREEN;

  ED_area_tag_redraw(area);

  /* send space change notifier */
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_CHANGED, area);
}

/* returns offset for next button in header */
int ED_area_header_switchbutton(const bContext *C, uiBlock *block, int yco)
{
  ScrArea *area = CTX_wm_area(C);
  bScreen *screen = CTX_wm_screen(C);
  PointerRNA areaptr;
  int xco = 0.4 * U.widget_unit;

  RNA_pointer_create(&(screen->id), &RNA_Area, area, &areaptr);

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
      if (ED_screen_area_active(C) || ED_area_is_global(area)) {
        return TH_HEADER;
      }
      else {
        return TH_HEADERDESEL;
      }
    case RGN_TYPE_PREVIEW:
      return TH_PREVIEW_BACK;
    default:
      return TH_BACK;
  }
}

static void region_clear_color(const bContext *C, const ARegion *region, ThemeColorID colorid)
{
  if (region->alignment == RGN_ALIGN_FLOAT) {
    /* handle our own drawing. */
  }
  else if (region->overlap) {
    /* view should be in pixelspace */
    UI_view2d_view_restore(C);

    float back[4];
    UI_GetThemeColor4fv(colorid, back);
    GPU_clear_color(back[3] * back[0], back[3] * back[1], back[3] * back[2], back[3]);
    GPU_clear(GPU_COLOR_BIT);
  }
  else {
    UI_ThemeClearColor(colorid);
    GPU_clear(GPU_COLOR_BIT);
  }
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
 * \param panel The panel to draw. Can be null, in which case a panel with the type of \a pt will
 * be created.
 * \param unique_panel_str A unique identifier for the name of the \a uiBlock associated with the
 * panel. Used when the panel is an instanced panel so a unique identifier is needed to find the
 * correct old \a uiBlock, and NULL otherwise.
 */
static void ed_panel_draw(const bContext *C,
                          ScrArea *area,
                          ARegion *region,
                          ListBase *lb,
                          PanelType *pt,
                          Panel *panel,
                          int w,
                          int em,
                          bool vertical,
                          char *unique_panel_str)
{
  const uiStyle *style = UI_style_get_dpi();

  /* Draw panel. */

  char block_name[BKE_ST_MAXNAME + LIST_PANEL_UNIQUE_STR_LEN];
  strncpy(block_name, pt->idname, BKE_ST_MAXNAME);
  if (unique_panel_str != NULL) {
    /* Instanced panels should have already been added at this point. */
    strncat(block_name, unique_panel_str, LIST_PANEL_UNIQUE_STR_LEN);
  }
  uiBlock *block = UI_block_begin(C, region, block_name, UI_EMBOSS);

  bool open;
  panel = UI_panel_begin(area, region, lb, block, pt, panel, &open);

  /* bad fixed values */
  int xco, yco, h = 0;
  int headerend = w - UI_UNIT_X;

  if (pt->draw_header_preset && !(pt->flag & PNL_NO_HEADER) && (open || vertical)) {
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

    pt->draw_header_preset(C, panel);

    UI_block_layout_resolve(block, &xco, &yco);
    UI_block_translate(block, headerend - xco, 0);
    panel->layout = NULL;
  }

  if (pt->draw_header && !(pt->flag & PNL_NO_HEADER) && (open || vertical)) {
    int labelx, labely;
    UI_panel_label_offset(block, &labelx, &labely);

    /* Unusual case: Use expanding layout (buttons stretch to available width). */
    if (pt->flag & PNL_LAYOUT_HEADER_EXPAND) {
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

    pt->draw_header(C, panel);

    UI_block_layout_resolve(block, &xco, &yco);
    panel->labelofs = xco - labelx;
    panel->layout = NULL;
  }
  else {
    panel->labelofs = 0;
  }

  if (open) {
    short panelContext;

    /* panel context can either be toolbar region or normal panels region */
    if (pt->flag & PNL_LAYOUT_VERT_BAR) {
      panelContext = UI_LAYOUT_VERT_BAR;
    }
    else if (region->regiontype == RGN_TYPE_TOOLS) {
      panelContext = UI_LAYOUT_TOOLBAR;
    }
    else {
      panelContext = UI_LAYOUT_PANEL;
    }

    panel->layout = UI_block_layout(block,
                                    UI_LAYOUT_VERTICAL,
                                    panelContext,
                                    (pt->flag & PNL_LAYOUT_VERT_BAR) ? 0 : style->panelspace,
                                    0,
                                    (pt->flag & PNL_LAYOUT_VERT_BAR) ? 0 :
                                                                       w - 2 * style->panelspace,
                                    em,
                                    0,
                                    style);

    pt->draw(C, panel);

    UI_block_layout_resolve(block, &xco, &yco);
    panel->layout = NULL;

    if (yco != 0) {
      h = -yco + 2 * style->panelspace;
    }
  }

  UI_block_end(C, block);

  /* Draw child panels. */
  if (open) {
    LISTBASE_FOREACH (LinkData *, link, &pt->children) {
      PanelType *child_pt = link->data;
      Panel *child_panel = UI_panel_find_by_type(&panel->children, child_pt);

      if (child_pt->draw && (!child_pt->poll || child_pt->poll(C, child_pt))) {
        ed_panel_draw(C,
                      area,
                      region,
                      &panel->children,
                      child_pt,
                      child_panel,
                      w,
                      em,
                      vertical,
                      unique_panel_str);
      }
    }
  }

  UI_panel_end(area, region, block, w, h, open);
}

/**
 * \param contexts: A NULL terminated array of context strings to match against.
 * Matching against any of these strings will draw the panel.
 * Can be NULL to skip context checks.
 */
void ED_region_panels_layout_ex(const bContext *C,
                                ARegion *region,
                                ListBase *paneltypes,
                                const char *contexts[],
                                int contextnr,
                                const bool vertical,
                                const char *category_override)
{
  /* collect panels to draw */
  WorkSpace *workspace = CTX_wm_workspace(C);
  LinkNode *panel_types_stack = NULL;
  for (PanelType *pt = paneltypes->last; pt; pt = pt->prev) {
    /* Only draw top level panels. */
    if (pt->parent) {
      continue;
    }

    if (category_override) {
      if (!STREQ(pt->category, category_override)) {
        continue;
      }
    }

    /* verify context */
    if (contexts && pt->context[0] && !streq_array_any(pt->context, contexts)) {
      continue;
    }

    /* If we're tagged, only use compatible. */
    if (pt->owner_id[0] && BKE_workspace_owner_id_check(workspace, pt->owner_id) == false) {
      continue;
    }

    /* draw panel */
    if (pt->draw && (!pt->poll || pt->poll(C, pt))) {
      BLI_linklist_prepend_alloca(&panel_types_stack, pt);
    }
  }

  region->runtime.category = NULL;

  ScrArea *area = CTX_wm_area(C);
  View2D *v2d = &region->v2d;
  int x, y, w, em;

  /* XXX, should use some better check? */
  /* For now also has hardcoded check for clip editor until it supports actual toolbar. */
  bool use_category_tabs = (category_override == NULL) &&
                           ((((1 << region->regiontype) & RGN_TYPE_HAS_CATEGORY_MASK) ||
                             (region->regiontype == RGN_TYPE_TOOLS &&
                              area->spacetype == SPACE_CLIP)));
  /* offset panels for small vertical tab area */
  const char *category = NULL;
  const int category_tabs_width = UI_PANEL_CATEGORY_MARGIN_WIDTH;
  int margin_x = 0;
  const bool region_layout_based = region->flag & RGN_FLAG_DYNAMIC_SIZE;
  const bool is_context_new = (contextnr != -1) ? UI_view2d_tab_set(v2d, contextnr) : false;
  bool update_tot_size = true;

  /* before setting the view */
  if (vertical) {
    /* only allow scrolling in vertical direction */
    v2d->keepofs |= V2D_LOCKOFS_X | V2D_KEEPOFS_Y;
    v2d->keepofs &= ~(V2D_LOCKOFS_Y | V2D_KEEPOFS_X);
    v2d->scroll &= ~V2D_SCROLL_BOTTOM;
    v2d->scroll |= V2D_SCROLL_RIGHT;
  }
  else {
    /* for now, allow scrolling in both directions (since layouts are optimized for vertical,
     * they often don't fit in horizontal layout)
     */
    v2d->keepofs &= ~(V2D_LOCKOFS_X | V2D_LOCKOFS_Y | V2D_KEEPOFS_X | V2D_KEEPOFS_Y);
    v2d->scroll |= V2D_SCROLL_BOTTOM;
    v2d->scroll &= ~V2D_SCROLL_RIGHT;
  }

  /* collect categories */
  if (use_category_tabs) {
    UI_panel_category_clear_all(region);

    /* gather unique categories */
    for (LinkNode *pt_link = panel_types_stack; pt_link; pt_link = pt_link->next) {
      PanelType *pt = pt_link->link;
      if (pt->category[0]) {
        if (!UI_panel_category_find(region, pt->category)) {
          UI_panel_category_add(region, pt->category);
        }
      }
    }

    if (!UI_panel_category_is_visible(region)) {
      use_category_tabs = false;
    }
    else {
      category = UI_panel_category_active_get(region, true);
      margin_x = category_tabs_width;
    }
  }

  if (vertical) {
    w = BLI_rctf_size_x(&v2d->cur);
    em = (region->type->prefsizex) ? 10 : 20; /* works out to 10*UI_UNIT_X or 20*UI_UNIT_X */
  }
  else {
    w = UI_PANEL_WIDTH;
    em = (region->type->prefsizex) ? 10 : 20;
  }

  w -= margin_x;
  int w_box_panel = w - UI_PANEL_BOX_STYLE_MARGIN * 2.0f;

  /* create panels */
  UI_panels_begin(C, region);

  /* set view2d view matrix  - UI_block_begin() stores it */
  UI_view2d_view_ortho(v2d);

  bool has_instanced_panel = false;
  for (LinkNode *pt_link = panel_types_stack; pt_link; pt_link = pt_link->next) {
    PanelType *pt = pt_link->link;

    if (pt->flag & PNL_INSTANCED) {
      has_instanced_panel = true;
      continue;
    }
    Panel *panel = UI_panel_find_by_type(&region->panels, pt);

    if (use_category_tabs && pt->category[0] && !STREQ(category, pt->category)) {
      if ((panel == NULL) || ((panel->flag & PNL_PIN) == 0)) {
        continue;
      }
    }

    if (panel && UI_panel_is_dragging(panel)) {
      /* Prevent View2d.tot rectangle size changes while dragging panels. */
      update_tot_size = false;
    }

    ed_panel_draw(C,
                  area,
                  region,
                  &region->panels,
                  pt,
                  panel,
                  (pt->flag & PNL_DRAW_BOX) ? w_box_panel : w,
                  em,
                  vertical,
                  NULL);
  }

  /* Draw "polyinstantaited" panels that don't have a 1 to 1 correspondence with their types. */
  if (has_instanced_panel) {
    LISTBASE_FOREACH (Panel *, panel, &region->panels) {
      if (panel->type == NULL) {
        continue; /* Some panels don't have a type.. */
      }
      if (panel->type->flag & PNL_INSTANCED) {
        if (panel && UI_panel_is_dragging(panel)) {
          /* Prevent View2d.tot rectangle size changes while dragging panels. */
          update_tot_size = false;
        }

        /* Use a unique identifier for instanced panels, otherwise an old block for a different
         * panel of the same type might be found. */
        char unique_panel_str[8];
        UI_list_panel_unique_str(panel, unique_panel_str);
        ed_panel_draw(C,
                      area,
                      region,
                      &region->panels,
                      panel->type,
                      panel,
                      (panel->type->flag & PNL_DRAW_BOX) ? w_box_panel : w,
                      em,
                      vertical,
                      unique_panel_str);
      }
    }
  }

  /* align panels and return size */
  UI_panels_end(C, region, &x, &y);

  /* before setting the view */
  if (region_layout_based) {
    /* XXX, only single panel support atm.
     * Can't use x/y values calculated above because they're not using the real height of panels,
     * instead they calculate offsets for the next panel to start drawing. */
    Panel *panel = region->panels.last;
    if (panel != NULL) {
      const int size_dyn[2] = {
          UI_UNIT_X * ((panel->flag & PNL_CLOSED) ? 8 : 14) / UI_DPI_FAC,
          UI_panel_size_y(panel) / UI_DPI_FAC,
      };
      /* region size is layout based and needs to be updated */
      if ((region->sizex != size_dyn[0]) || (region->sizey != size_dyn[1])) {
        region->sizex = size_dyn[0];
        region->sizey = size_dyn[1];
        area->flag |= AREA_FLAG_REGION_SIZE_UPDATE;
      }
      y = fabsf(region->sizey * UI_DPI_FAC - 1);
    }
  }
  else if (vertical) {
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
  else {
    /* don't jump back when panels close or hide */
    if (!is_context_new) {
      if (v2d->tot.xmax > v2d->winx) {
        x = max_ii(x, 0);
      }
      else {
        x = max_ii(x, v2d->cur.xmax);
      }
    }

    y = -y;
  }

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
  bool vertical = true;
  ED_region_panels_layout_ex(C, region, &region->type->paneltypes, NULL, -1, vertical, NULL);
}

void ED_region_panels_draw(const bContext *C, ARegion *region)
{
  View2D *v2d = &region->v2d;

  if (region->alignment != RGN_ALIGN_FLOAT) {
    region_clear_color(
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
  const rcti *mask = NULL;
  rcti mask_buf;
  if (region->runtime.category &&
      (RGN_ALIGN_ENUM_FROM_MASK(region->alignment) == RGN_ALIGN_RIGHT)) {
    UI_view2d_mask_from_win(v2d, &mask_buf);
    mask_buf.xmax -= UI_PANEL_CATEGORY_MARGIN_WIDTH;
    mask = &mask_buf;
  }
  View2DScrollers *scrollers = UI_view2d_scrollers_calc(v2d, mask);
  UI_view2d_scrollers_draw(v2d, scrollers);
  UI_view2d_scrollers_free(scrollers);
}

void ED_region_panels_ex(
    const bContext *C, ARegion *region, const char *contexts[], int contextnr, const bool vertical)
{
  /* TODO: remove? */
  ED_region_panels_layout_ex(
      C, region, &region->type->paneltypes, contexts, contextnr, vertical, NULL);
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
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_PANELS_UI, region->winx, region->winy);

  keymap = WM_keymap_ensure(wm->defaultconf, "View2D Buttons List", 0, 0);
  WM_event_add_keymap_handler(&region->handlers, keymap);
}

void ED_region_header_layout(const bContext *C, ARegion *region)
{
  const uiStyle *style = UI_style_get_dpi();
  uiBlock *block;
  uiLayout *layout;
  HeaderType *ht;
  Header header = {NULL};
  bool region_layout_based = region->flag & RGN_FLAG_DYNAMIC_SIZE;

  /* Height of buttons and scaling needed to achieve it. */
  const int buttony = min_ii(UI_UNIT_Y, region->winy - 2 * UI_DPI_FAC);
  const float buttony_scale = buttony / (float)UI_UNIT_Y;

  /* Vertically center buttons. */
  int xco = UI_HEADER_OFFSET;
  int yco = buttony + (region->winy - buttony) / 2;
  int maxco = xco;

  /* XXX workaround for 1 px alignment issue. Not sure what causes it...
   * Would prefer a proper fix - Julian */
  if (!ELEM(CTX_wm_area(C)->spacetype, SPACE_TOPBAR, SPACE_STATUSBAR)) {
    yco -= 1;
  }

  /* set view2d view matrix for scrolling (without scrollers) */
  UI_view2d_view_ortho(&region->v2d);

  /* draw all headers types */
  for (ht = region->type->headertypes.first; ht; ht = ht->next) {
    if (ht->poll && !ht->poll(C, ht)) {
      continue;
    }

    block = UI_block_begin(C, region, ht->idname, UI_EMBOSS);
    layout = UI_block_layout(
        block, UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER, xco, yco, buttony, 1, 0, style);

    if (buttony_scale != 1.0f) {
      uiLayoutSetScaleY(layout, buttony_scale);
    }

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

    int new_sizex = (maxco + UI_HEADER_OFFSET) / UI_DPI_FAC;

    if (region_layout_based && (region->sizex != new_sizex)) {
      /* region size is layout based and needs to be updated */
      ScrArea *area = CTX_wm_area(C);

      region->sizex = new_sizex;
      area->flag |= AREA_FLAG_REGION_SIZE_UPDATE;
    }

    UI_block_end(C, block);
  }

  if (!region_layout_based) {
    maxco += UI_HEADER_OFFSET;
  }

  /* always as last  */
  UI_view2d_totRect_set(&region->v2d, maxco, region->winy);

  /* restore view matrix */
  UI_view2d_view_restore(C);
}

void ED_region_header_draw(const bContext *C, ARegion *region)
{
  /* clear */
  region_clear_color(C, region, region_background_color_id(C, region));

  UI_view2d_view_ortho(&region->v2d);

  /* View2D matrix might have changed due to dynamic sized regions. */
  UI_blocklist_update_window_matrix(C, &region->uiblocks);

  /* draw blocks */
  UI_blocklist_draw(C, &region->uiblocks);

  /* restore view matrix */
  UI_view2d_view_restore(C);
}

void ED_region_header(const bContext *C, ARegion *region)
{
  /* TODO: remove? */
  ED_region_header_layout(C, region);
  ED_region_header_draw(C, region);
}

void ED_region_header_init(ARegion *region)
{
  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_HEADER, region->winx, region->winy);
}

int ED_area_headersize(void)
{
  /* Accommodate widget and padding. */
  return U.widget_unit + (int)(UI_DPI_FAC * HEADER_PADDING_Y);
}

int ED_area_header_alignment_or_fallback(const ScrArea *area, int fallback)
{
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_HEADER) {
      return region->alignment;
    }
  }
  return fallback;
}

int ED_area_header_alignment(const ScrArea *area)
{
  return ED_area_header_alignment_or_fallback(
      area, (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP);
}

int ED_area_footersize(void)
{
  return ED_area_headersize();
}

int ED_area_footer_alignment_or_fallback(const ScrArea *area, int fallback)
{
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == RGN_TYPE_FOOTER) {
      return region->alignment;
    }
  }
  return fallback;
}

int ED_area_footer_alignment(const ScrArea *area)
{
  return ED_area_footer_alignment_or_fallback(
      area, (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM);
}

/**
 * \return the final height of a global \a area, accounting for DPI.
 */
int ED_area_global_size_y(const ScrArea *area)
{
  BLI_assert(ED_area_is_global(area));
  return round_fl_to_int(area->global->cur_fixed_height * UI_DPI_FAC);
}
int ED_area_global_min_size_y(const ScrArea *area)
{
  BLI_assert(ED_area_is_global(area));
  return round_fl_to_int(area->global->size_min * UI_DPI_FAC);
}
int ED_area_global_max_size_y(const ScrArea *area)
{
  BLI_assert(ED_area_is_global(area));
  return round_fl_to_int(area->global->size_max * UI_DPI_FAC);
}

bool ED_area_is_global(const ScrArea *area)
{
  return area->global != NULL;
}

ScrArea *ED_screen_areas_iter_first(const wmWindow *win, const bScreen *screen)
{
  ScrArea *global_area = win->global_areas.areabase.first;

  if (!global_area) {
    return screen->areabase.first;
  }
  else if ((global_area->global->flag & GLOBAL_AREA_IS_HIDDEN) == 0) {
    return global_area;
  }
  /* Find next visible area. */
  return ED_screen_areas_iter_next(screen, global_area);
}
ScrArea *ED_screen_areas_iter_next(const bScreen *screen, const ScrArea *area)
{
  if (area->global) {
    for (ScrArea *area_iter = area->next; area_iter; area_iter = area_iter->next) {
      if ((area_iter->global->flag & GLOBAL_AREA_IS_HIDDEN) == 0) {
        return area_iter;
      }
    }
    /* No visible next global area found, start iterating over layout areas. */
    return screen->areabase.first;
  }

  return area->next;
}

/**
 * For now we just assume all global areas are made up out of horizontal bars
 * with the same size. A fixed size could be stored in ARegion instead if needed.
 *
 * \return the DPI aware height of a single bar/region in global areas.
 */
int ED_region_global_size_y(void)
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
  GPU_scissor_get_i(scissor);
  GPU_scissor(rect.xmin, rect.ymin, BLI_rcti_size_x(&rect) + 1, BLI_rcti_size_y(&rect) + 1);

  GPU_blend(true);
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor4fv(fill_color);
  immRecti(pos, rect.xmin, rect.ymin, rect.xmax + 1, rect.ymax + 1);
  immUnbindProgram();
  GPU_blend(false);

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
  const char *text_array[2] = {text, NULL};
  ED_region_info_draw_multiline(region, text_array, fill_color, full_redraw);
}

#define MAX_METADATA_STR 1024

static const char *meta_data_list[] = {
    "File",
    "Strip",
    "Date",
    "RenderTime",
    "Note",
    "Marker",
    "Time",
    "Frame",
    "Camera",
    "Scene",
};

BLI_INLINE bool metadata_is_valid(ImBuf *ibuf, char *r_str, short index, int offset)
{
  return (IMB_metadata_get_field(
              ibuf->metadata, meta_data_list[index], r_str + offset, MAX_METADATA_STR - offset) &&
          r_str[0]);
}

BLI_INLINE bool metadata_is_custom_drawable(const char *field)
{
  /* Metadata field stored by Blender for multilayer EXR images. Is rather
   * useless to be viewed all the time. Can still be seen in the Metadata
   * panel. */
  if (STREQ(field, "BlenderMultiChannel")) {
    return false;
  }
  /* Is almost always has value "scanlineimage", also useless to be seen
   * all the time. */
  if (STREQ(field, "type")) {
    return false;
  }
  return !BKE_stamp_is_known_field(field);
}

typedef struct MetadataCustomDrawContext {
  int fontid;
  int xmin, ymin;
  int vertical_offset;
  int current_y;
} MetadataCustomDrawContext;

static void metadata_custom_draw_fields(const char *field, const char *value, void *ctx_v)
{
  if (!metadata_is_custom_drawable(field)) {
    return;
  }
  MetadataCustomDrawContext *ctx = (MetadataCustomDrawContext *)ctx_v;
  char temp_str[MAX_METADATA_STR];
  BLI_snprintf(temp_str, MAX_METADATA_STR, "%s: %s", field, value);
  BLF_position(ctx->fontid, ctx->xmin, ctx->ymin + ctx->current_y, 0.0f);
  BLF_draw(ctx->fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
  ctx->current_y += ctx->vertical_offset;
}

static void metadata_draw_imbuf(ImBuf *ibuf, const rctf *rect, int fontid, const bool is_top)
{
  char temp_str[MAX_METADATA_STR];
  int line_width;
  int ofs_y = 0;
  short i;
  int len;
  const float height = BLF_height_max(fontid);
  const float margin = height / 8;
  const float vertical_offset = (height + margin);

  /* values taking margins into account */
  const float descender = BLF_descender(fontid);
  const float xmin = (rect->xmin + margin);
  const float xmax = (rect->xmax - margin);
  const float ymin = (rect->ymin + margin) - descender;
  const float ymax = (rect->ymax - margin) - descender;

  if (is_top) {
    for (i = 0; i < 4; i++) {
      /* first line */
      if (i == 0) {
        bool do_newline = false;
        len = BLI_snprintf_rlen(temp_str, MAX_METADATA_STR, "%s: ", meta_data_list[0]);
        if (metadata_is_valid(ibuf, temp_str, 0, len)) {
          BLF_position(fontid, xmin, ymax - vertical_offset, 0.0f);
          BLF_draw(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
          do_newline = true;
        }

        len = BLI_snprintf_rlen(temp_str, MAX_METADATA_STR, "%s: ", meta_data_list[1]);
        if (metadata_is_valid(ibuf, temp_str, 1, len)) {
          line_width = BLF_width(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
          BLF_position(fontid, xmax - line_width, ymax - vertical_offset, 0.0f);
          BLF_draw(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
          do_newline = true;
        }

        if (do_newline) {
          ofs_y += vertical_offset;
        }
      } /* Strip */
      else if (i == 1 || i == 2) {
        len = BLI_snprintf_rlen(temp_str, MAX_METADATA_STR, "%s: ", meta_data_list[i + 1]);
        if (metadata_is_valid(ibuf, temp_str, i + 1, len)) {
          BLF_position(fontid, xmin, ymax - vertical_offset - ofs_y, 0.0f);
          BLF_draw(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
          ofs_y += vertical_offset;
        }
      } /* Note (wrapped) */
      else if (i == 3) {
        len = BLI_snprintf_rlen(temp_str, MAX_METADATA_STR, "%s: ", meta_data_list[i + 1]);
        if (metadata_is_valid(ibuf, temp_str, i + 1, len)) {
          struct ResultBLF info;
          BLF_enable(fontid, BLF_WORD_WRAP);
          BLF_wordwrap(fontid, ibuf->x - (margin * 2));
          BLF_position(fontid, xmin, ymax - vertical_offset - ofs_y, 0.0f);
          BLF_draw_ex(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX, &info);
          BLF_wordwrap(fontid, 0);
          BLF_disable(fontid, BLF_WORD_WRAP);
          ofs_y += vertical_offset * info.lines;
        }
      }
      else {
        len = BLI_snprintf_rlen(temp_str, MAX_METADATA_STR, "%s: ", meta_data_list[i + 1]);
        if (metadata_is_valid(ibuf, temp_str, i + 1, len)) {
          line_width = BLF_width(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
          BLF_position(fontid, xmax - line_width, ymax - vertical_offset - ofs_y, 0.0f);
          BLF_draw(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);
          ofs_y += vertical_offset;
        }
      }
    }
  }
  else {
    MetadataCustomDrawContext ctx;
    ctx.fontid = fontid;
    ctx.xmin = xmin;
    ctx.ymin = ymin;
    ctx.current_y = ofs_y;
    ctx.vertical_offset = vertical_offset;
    IMB_metadata_foreach(ibuf, metadata_custom_draw_fields, &ctx);
    int ofs_x = 0;
    ofs_y = ctx.current_y;
    for (i = 5; i < 10; i++) {
      len = BLI_snprintf_rlen(temp_str, MAX_METADATA_STR, "%s: ", meta_data_list[i]);
      if (metadata_is_valid(ibuf, temp_str, i, len)) {
        BLF_position(fontid, xmin + ofs_x, ymin + ofs_y, 0.0f);
        BLF_draw(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX);

        ofs_x += BLF_width(fontid, temp_str, BLF_DRAW_STR_DUMMY_MAX) + UI_UNIT_X;
      }
    }
  }
}

typedef struct MetadataCustomCountContext {
  int count;
} MetadataCustomCountContext;

static void metadata_custom_count_fields(const char *field, const char *UNUSED(value), void *ctx_v)
{
  if (!metadata_is_custom_drawable(field)) {
    return;
  }
  MetadataCustomCountContext *ctx = (MetadataCustomCountContext *)ctx_v;
  ctx->count++;
}

static float metadata_box_height_get(ImBuf *ibuf, int fontid, const bool is_top)
{
  const float height = BLF_height_max(fontid);
  const float margin = (height / 8);
  char str[MAX_METADATA_STR] = "";
  short i, count = 0;

  if (is_top) {
    if (metadata_is_valid(ibuf, str, 0, 0) || metadata_is_valid(ibuf, str, 1, 0)) {
      count++;
    }
    for (i = 2; i < 5; i++) {
      if (metadata_is_valid(ibuf, str, i, 0)) {
        if (i == 4) {
          struct {
            struct ResultBLF info;
            rctf rect;
          } wrap;

          BLF_enable(fontid, BLF_WORD_WRAP);
          BLF_wordwrap(fontid, ibuf->x - (margin * 2));
          BLF_boundbox_ex(fontid, str, sizeof(str), &wrap.rect, &wrap.info);
          BLF_wordwrap(fontid, 0);
          BLF_disable(fontid, BLF_WORD_WRAP);

          count += wrap.info.lines;
        }
        else {
          count++;
        }
      }
    }
  }
  else {
    for (i = 5; i < 10; i++) {
      if (metadata_is_valid(ibuf, str, i, 0)) {
        count = 1;
        break;
      }
    }
    MetadataCustomCountContext ctx;
    ctx.count = 0;
    IMB_metadata_foreach(ibuf, metadata_custom_count_fields, &ctx);
    count += ctx.count;
  }

  if (count) {
    return (height + margin) * count;
  }

  return 0;
}

#undef MAX_METADATA_STR

void ED_region_image_metadata_draw(
    int x, int y, ImBuf *ibuf, const rctf *frame, float zoomx, float zoomy)
{
  float box_y;
  rctf rect;
  const uiStyle *style = UI_style_get_dpi();

  if (!ibuf->metadata) {
    return;
  }

  /* find window pixel coordinates of origin */
  GPU_matrix_push();

  /* offset and zoom using ogl */
  GPU_matrix_translate_2f(x, y);
  GPU_matrix_scale_2f(zoomx, zoomy);

  BLF_size(blf_mono_font, style->widgetlabel.points * 1.5f * U.pixelsize, U.dpi);

  /* *** upper box*** */

  /* get needed box height */
  box_y = metadata_box_height_get(ibuf, blf_mono_font, true);

  if (box_y) {
    /* set up rect */
    BLI_rctf_init(&rect, frame->xmin, frame->xmax, frame->ymax, frame->ymax + box_y);
    /* draw top box */
    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformThemeColor(TH_METADATA_BG);
    immRectf(pos, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
    immUnbindProgram();

    BLF_clipping(blf_mono_font, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
    BLF_enable(blf_mono_font, BLF_CLIPPING);

    UI_FontThemeColor(blf_mono_font, TH_METADATA_TEXT);
    metadata_draw_imbuf(ibuf, &rect, blf_mono_font, true);

    BLF_disable(blf_mono_font, BLF_CLIPPING);
  }

  /* *** lower box*** */

  box_y = metadata_box_height_get(ibuf, blf_mono_font, false);

  if (box_y) {
    /* set up box rect */
    BLI_rctf_init(&rect, frame->xmin, frame->xmax, frame->ymin - box_y, frame->ymin);
    /* draw top box */
    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformThemeColor(TH_METADATA_BG);
    immRectf(pos, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
    immUnbindProgram();

    BLF_clipping(blf_mono_font, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
    BLF_enable(blf_mono_font, BLF_CLIPPING);

    UI_FontThemeColor(blf_mono_font, TH_METADATA_TEXT);
    metadata_draw_imbuf(ibuf, &rect, blf_mono_font, false);

    BLF_disable(blf_mono_font, BLF_CLIPPING);
  }

  GPU_matrix_pop();
}

typedef struct MetadataPanelDrawContext {
  uiLayout *layout;
} MetadataPanelDrawContext;

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
  float gridsize, gridstep = 1.0f / 32.0f;
  float fac, blendfac;
  int x1, y1, x2, y2;

  /* the image is located inside (x0, y0), (x0+1, y0+1) as set by view2d */
  UI_view2d_view_to_region(&region->v2d, x0, y0, &x1, &y1);
  UI_view2d_view_to_region(&region->v2d, x0 + 1.0f, y0 + 1.0f, &x2, &y2);

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformThemeColorShade(TH_BACK, 20);
  immRectf(pos, x1, y1, x2, y2);
  immUnbindProgram();

  /* gridsize adapted to zoom level */
  gridsize = 0.5f * (zoomx + zoomy);
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

  blendfac = 0.25f * gridsize - floorf(0.25f * gridsize);
  CLAMP(blendfac, 0.0f, 1.0f);

  int count_fine = 1.0f / gridstep;
  int count_large = 1.0f / (4.0f * gridstep);

  if (count_fine > 0) {
    GPU_vertformat_clear(format);
    pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

    immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);
    immBegin(GPU_PRIM_LINES, 4 * count_fine + 4 * count_large);

    float theme_color[3];
    UI_GetThemeColorShade3fv(TH_BACK, (int)(20.0f * (1.0f - blendfac)), theme_color);
    fac = 0.0f;

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
      UI_GetThemeColor3fv(TH_BACK, theme_color);
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
  ARegion *arn = region;

  /* allow function to be called without area */
  while (arn->prev) {
    arn = arn->prev;
  }

  *rect = region->winrct;

  /* check if a region overlaps with the current one */
  for (; arn; arn = arn->next) {
    if (region != arn && arn->overlap) {
      if (BLI_rcti_isect(rect, &arn->winrct, NULL)) {
        int alignment = RGN_ALIGN_ENUM_FROM_MASK(arn->alignment);

        if (ELEM(alignment, RGN_ALIGN_LEFT, RGN_ALIGN_RIGHT)) {
          /* Overlap left, also check 1 pixel offset (2 regions on one side). */
          if (abs(rect->xmin - arn->winrct.xmin) < 2) {
            rect->xmin = arn->winrct.xmax;
          }

          /* Overlap right. */
          if (abs(rect->xmax - arn->winrct.xmax) < 2) {
            rect->xmax = arn->winrct.xmin;
          }
        }
        else if (ELEM(alignment, RGN_ALIGN_TOP, RGN_ALIGN_BOTTOM)) {
          /* Same logic as above for vertical regions. */
          if (abs(rect->ymin - arn->winrct.ymin) < 2) {
            rect->ymin = arn->winrct.ymax;
          }
          if (abs(rect->ymax - arn->winrct.ymax) < 2) {
            rect->ymax = arn->winrct.ymin;
          }
        }
        else if (alignment == RGN_ALIGN_FLOAT) {
          /* Skip floating. */
        }
        else {
          BLI_assert(!"Region overlap with unknown alignment");
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
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
  immUniformColor4ub(128, 128, 255, 64);
  immRecti(pos, 0, region_bottom, region->winx, region_bottom + 8 * UI_DPI_FAC);
  immUnbindProgram();
}

void ED_region_cache_draw_curfra_label(const int framenr, const float x, const float y)
{
  const uiStyle *style = UI_style_get();
  int fontid = style->widget.uifont_id;
  char numstr[32];
  float font_dims[2] = {0.0f, 0.0f};

  /* frame number */
  BLF_size(fontid, 11.0f * U.pixelsize, U.dpi);
  BLI_snprintf(numstr, sizeof(numstr), "%d", framenr);

  BLF_width_and_height(fontid, numstr, sizeof(numstr), &font_dims[0], &font_dims[1]);

  uint pos = GPU_vertformat_attr_add(
      immVertexFormat(), "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
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
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
    immUniformColor4ub(128, 128, 255, 128);

    for (int a = 0; a < num_segments; a++) {
      float x1 = (float)(points[a * 2] - sfra) / (efra - sfra + 1) * region->winx;
      float x2 = (float)(points[a * 2 + 1] - sfra + 1) / (efra - sfra + 1) * region->winx;

      immRecti(pos, x1, region_bottom, x2, region_bottom + 8 * UI_DPI_FAC);
      /* TODO(merwin): use primitive restart to draw multiple rects more efficiently */
    }

    immUnbindProgram();
  }
}

/**
 * Generate subscriptions for this region.
 */
void ED_region_message_subscribe(bContext *C,
                                 struct WorkSpace *workspace,
                                 struct Scene *scene,
                                 struct bScreen *screen,
                                 struct ScrArea *area,
                                 struct ARegion *region,
                                 struct wmMsgBus *mbus)
{
  if (region->gizmo_map != NULL) {
    WM_gizmomap_message_subscribe(C, region->gizmo_map, region, mbus);
  }

  if (!BLI_listbase_is_empty(&region->uiblocks)) {
    UI_region_message_subscribe(region, mbus);
  }

  if (region->type->message_subscribe != NULL) {
    region->type->message_subscribe(C, workspace, scene, screen, area, region, mbus);
  }
}

int ED_region_snap_size_test(const ARegion *region)
{
  /* Use a larger value because toggling scrollbars can jump in size. */
  const int snap_match_threshold = 16;
  if (region->type->snap_size != NULL) {
    return ((((region->sizex - region->type->snap_size(region, region->sizex, 0)) <=
              snap_match_threshold)
             << 0) |
            (((region->sizey - region->type->snap_size(region, region->sizey, 1)) <=
              snap_match_threshold)
             << 1));
  }
  return 0;
}

bool ED_region_snap_size_apply(ARegion *region, int snap_flag)
{
  bool changed = false;
  if (region->type->snap_size != NULL) {
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
