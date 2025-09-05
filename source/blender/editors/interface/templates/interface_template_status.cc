/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <fmt/format.h>

#include "BKE_blender_version.h"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "BLF_api.hh"
#include "BLT_translation.hh"

#include "DNA_space_types.h"
#include "DNA_workspace_types.h"

#include "ED_info.hh"
#include "ED_screen_types.hh"

#include "WM_api.hh"

#include "UI_interface_layout.hh"
#include "interface_intern.hh"

/* Maximum width for a Status Bar report */
#define REPORT_BANNER_MAX_WIDTH (800.0f * UI_SCALE_FAC)

void uiTemplateReportsBanner(uiLayout *layout, bContext *C)
{
  ReportList *reports = CTX_wm_reports(C);
  Report *report = BKE_reports_last_displayable(reports);
  const uiStyle *style = UI_style_get();

  uiBut *but;

  /* if the report display has timed out, don't show */
  if (!reports->reporttimer) {
    return;
  }

  ReportTimerInfo *rti = (ReportTimerInfo *)reports->reporttimer->customdata;

  if (!rti || rti->widthfac == 0.0f || !report) {
    return;
  }

  uiLayout *ui_abs = &layout->absolute(false);
  uiBlock *block = ui_abs->block();
  blender::ui::EmbossType previous_emboss = UI_block_emboss_get(block);

  uchar report_icon_color[4];
  uchar report_text_color[4];

  UI_GetThemeColorType4ubv(
      UI_icon_colorid_from_report_type(report->type), SPACE_INFO, report_icon_color);
  UI_GetThemeColorType4ubv(
      UI_text_colorid_from_report_type(report->type), SPACE_INFO, report_text_color);
  report_text_color[3] = 255; /* This theme color is RGB only, so have to set alpha here. */

  if (rti->flash_progress <= 1.0) {
    /* Flash report briefly according to progress through fade-out duration. */
    const int brighten_amount = int(32 * (1.0f - rti->flash_progress));
    add_v3_uchar_clamped(report_icon_color, brighten_amount);
  }

  UI_fontstyle_set(&style->widget);
  int width = BLF_width(style->widget.uifont_id, report->message, report->len);
  width = min_ii(width, int(REPORT_BANNER_MAX_WIDTH));
  width = min_ii(int(rti->widthfac * width), width);
  width = max_ii(width, 10 * UI_SCALE_FAC);

  UI_block_align_begin(block);

  /* Background for icon. */
  but = uiDefBut(block,
                 ButType::Roundbox,
                 0,
                 "",
                 0,
                 0,
                 UI_UNIT_X + (6 * UI_SCALE_FAC),
                 UI_UNIT_Y,
                 nullptr,
                 0.0f,
                 0.0f,
                 "");
  /* #ButType::Roundbox's background color is set in `but->col`. */
  copy_v4_v4_uchar(but->col, report_icon_color);

  /* Background for the rest of the message. */
  but = uiDefBut(block,
                 ButType::Roundbox,
                 0,
                 "",
                 UI_UNIT_X + (6 * UI_SCALE_FAC),
                 0,
                 UI_UNIT_X + width,
                 UI_UNIT_Y,
                 nullptr,
                 0.0f,
                 0.0f,
                 "");
  /* Use icon background at low opacity to highlight, but still contrasting with area TH_TEXT. */
  copy_v3_v3_uchar(but->col, report_icon_color);
  but->col[3] = 64;

  UI_block_align_end(block);
  UI_block_emboss_set(block, blender::ui::EmbossType::None);

  /* The report icon itself. */
  but = uiDefIconButO(block,
                      ButType::But,
                      "SCREEN_OT_info_log_show",
                      blender::wm::OpCallContext::InvokeRegionWin,
                      UI_icon_from_report_type(report->type),
                      (3 * UI_SCALE_FAC),
                      0,
                      UI_UNIT_X,
                      UI_UNIT_Y,
                      TIP_("Click to open the info editor"));
  copy_v4_v4_uchar(but->col, report_text_color);

  /* The report message. */
  but = uiDefButO(block,
                  ButType::But,
                  "SCREEN_OT_info_log_show",
                  blender::wm::OpCallContext::InvokeRegionWin,
                  report->message,
                  UI_UNIT_X,
                  0,
                  width + UI_UNIT_X,
                  UI_UNIT_Y,
                  TIP_("Show in Info Log"));

  UI_block_emboss_set(block, previous_emboss);
}

static bool uiTemplateInputStatusAzone(uiLayout *layout, const AZone *az, const ARegion *region)
{
  if (az->type == AZONE_AREA) {
    layout->label(nullptr, ICON_MOUSE_LMB_DRAG);
    layout->separator(-0.2f);
    layout->label(IFACE_("Split/Dock"), ICON_NONE);
    layout->separator(0.6f);
    layout->label("", ICON_EVENT_SHIFT);
    layout->separator(-0.4f);
    layout->label(nullptr, ICON_MOUSE_LMB_DRAG);
    layout->separator(-0.2f);
    layout->label(IFACE_("Duplicate into Window"), ICON_NONE);
    layout->separator(0.6f);
    layout->label("", ICON_EVENT_CTRL);
    layout->separator(ui_event_icon_offset(ICON_EVENT_CTRL));
    layout->label(nullptr, ICON_MOUSE_LMB_DRAG);
    layout->separator(-0.2f);
    layout->label(IFACE_("Swap Areas"), ICON_NONE);
    return true;
  }

  if (az->type == AZONE_REGION) {
    layout->label(nullptr, ICON_MOUSE_LMB_DRAG);
    layout->separator(-0.2f);
    layout->label((region->runtime->visible) ? IFACE_("Resize Region") :
                                               IFACE_("Show Hidden Region"),
                  ICON_NONE);
    return true;
  }

  return false;
}

static bool uiTemplateInputStatusBorder(wmWindow *win, uiLayout *row)
{
  /* On a gap between editors. */
  rcti win_rect;
  const int pad = int((3.0f * UI_SCALE_FAC) + U.pixelsize);
  WM_window_screen_rect_calc(win, &win_rect);
  BLI_rcti_pad(&win_rect, pad * -2, pad);
  if (BLI_rcti_isect_pt_v(&win_rect, win->eventstate->xy)) {
    /* Show options but not along left and right edges. */
    BLI_rcti_pad(&win_rect, 0, pad * -3);
    if (BLI_rcti_isect_pt_v(&win_rect, win->eventstate->xy)) {
      /* No resize at top and bottom. */
      row->label(nullptr, ICON_MOUSE_LMB_DRAG);
      row->separator(-0.2f);
      row->label(IFACE_("Resize"), ICON_NONE);
      row->separator(0.6f);
    }
    row->label(nullptr, ICON_MOUSE_RMB);
    row->separator(-0.9f);
    row->label(IFACE_("Options"), ICON_NONE);
    return true;
  }
  return false;
}

static bool uiTemplateInputStatusHeader(ARegion *region, uiLayout *row)
{
  if (region->regiontype != RGN_TYPE_HEADER) {
    return false;
  }
  /* Over a header region. */
  row->label(nullptr, ICON_MOUSE_MMB_DRAG);
  row->separator(-0.2f);
  row->label(IFACE_("Pan"), ICON_NONE);
  row->separator(0.6f);
  row->label(nullptr, ICON_MOUSE_RMB);
  row->separator(-0.9f);
  row->label(IFACE_("Options"), ICON_NONE);
  return true;
}

static bool uiTemplateInputStatus3DView(bContext *C, uiLayout *row)
{
  const Object *ob = CTX_data_active_object(C);
  if (!ob) {
    return false;
  }

  if (is_negative_m4(ob->object_to_world().ptr())) {
    row->separator(1.0f);
    row->label("", ICON_ERROR);
    row->separator(-0.2f);
    row->label(IFACE_("Active object has negative scale"), ICON_NONE);
    row->separator(0.5f, LayoutSeparatorType::Line);
    row->separator(0.5f);
    /* Return false to allow other items to be added after. */
    return false;
  }

  if (!(fabsf(ob->scale[0] - ob->scale[1]) < 1e-4f && fabsf(ob->scale[1] - ob->scale[2]) < 1e-4f))
  {
    row->separator(1.0f);
    row->label("", ICON_ERROR);
    row->separator(-0.2f);
    row->label(IFACE_("Active object has non-uniform scale"), ICON_NONE);
    row->separator(0.5f, LayoutSeparatorType::Line);
    row->separator(0.5f);
    /* Return false to allow other items to be added after. */
    return false;
  }

  return false;
}

void uiTemplateInputStatus(uiLayout *layout, bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  WorkSpace *workspace = CTX_wm_workspace(C);

  /* Workspace status text has priority. */
  if (!workspace->runtime->status.is_empty()) {
    uiLayout *row = &layout->row(true);
    for (const blender::bke::WorkSpaceStatusItem &item : workspace->runtime->status) {
      if (item.space_factor != 0.0f) {
        row->separator(item.space_factor);
      }
      else {
        uiBut *but = uiItemL_ex(row, item.text, item.icon, false, false);
        if (item.inverted) {
          but->drawflag |= UI_BUT_ICON_INVERT;
        }
        const float offset = ui_event_icon_offset(item.icon);
        if (offset != 0.0f) {
          row->separator(offset);
        }
      }
    }
    return;
  }

  if (WM_window_modal_keymap_status_draw(C, win, layout)) {
    return;
  }

  bScreen *screen = CTX_wm_screen(C);
  ARegion *region = screen->active_region;
  uiLayout *row = &layout->row(true);

  if (region == nullptr) {
    /* Check if over an action zone. */
    LISTBASE_FOREACH (ScrArea *, area_iter, &screen->areabase) {
      LISTBASE_FOREACH (AZone *, az, &area_iter->actionzones) {
        if (BLI_rcti_isect_pt_v(&az->rect, win->eventstate->xy)) {
          region = az->region;
          if (uiTemplateInputStatusAzone(row, az, region)) {
            return;
          }
          break;
        }
      }
    }
  }

  ScrArea *area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, win->eventstate->xy);
  if (!area) {
    /* Are we in a global area? */
    LISTBASE_FOREACH (ScrArea *, global_area, &win->global_areas.areabase) {
      if (BLI_rcti_isect_pt_v(&global_area->totrct, win->eventstate->xy)) {
        area = global_area;
        break;
      }
    }
  }

  if (!area) {
    /* Outside of all areas. */
    return;
  }

  if (!region && win && uiTemplateInputStatusBorder(win, row)) {
    /* On a gap between editors. */
    return;
  }

  if (region && uiTemplateInputStatusHeader(region, row)) {
    /* Over a header region. */
    return;
  }

  if (area && area->spacetype == SPACE_VIEW3D && uiTemplateInputStatus3DView(C, row)) {
    /* Specific to 3DView. */
    return;
  }

  if (!area || !region) {
    /* Keymap status only if over a region in an area. */
    return;
  }

  /* Otherwise should cursor keymap status. */
  for (int i = 0; i < 3; i++) {
    row->alignment_set(blender::ui::LayoutAlign::Left);

    const char *msg = CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT,
                                 WM_window_cursor_keymap_status_get(win, i, 0));
    const char *msg_drag = CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT,
                                      WM_window_cursor_keymap_status_get(win, i, 1));

    if (msg) {
      row->label("", (ICON_MOUSE_LMB + i));
      row->separator(-0.9f);
      row->label(msg, ICON_NONE);
      row->separator(0.6f);
    }

    if (msg_drag) {
      row->label("", (ICON_MOUSE_LMB_DRAG + i));
      row->separator(-0.4f);
      row->label(msg_drag, ICON_NONE);
      row->separator(0.6f);
    }
  }
}

static std::string ui_template_status_tooltip(bContext *C,
                                              void * /*argN*/,
                                              const blender::StringRef /*tip*/)
{
  Main *bmain = CTX_data_main(C);
  std::string tooltip_message;

  if (bmain->has_forward_compatibility_issues) {
    char writer_ver_str[12];
    BKE_blender_version_blendfile_string_from_values(
        writer_ver_str, sizeof(writer_ver_str), bmain->versionfile, -1);
    tooltip_message += fmt::format(
        fmt::runtime(RPT_("File saved by newer Blender\n({}), expect loss of data")),
        writer_ver_str);
  }
  if (bmain->is_asset_edit_file) {
    if (!tooltip_message.empty()) {
      tooltip_message += "\n\n";
    }
    tooltip_message += RPT_(
        "This file is managed by the Blender asset system and cannot be overridden");
  }
  if (bmain->colorspace.is_missing_opencolorio_config) {
    if (!tooltip_message.empty()) {
      tooltip_message += "\n\n";
    }
    tooltip_message += RPT_(
        "Displays, views or color spaces in this file were missing and have been changed");
  }

  return tooltip_message;
}

void uiTemplateStatusInfo(uiLayout *layout, bContext *C)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  uiLayout *row = &layout->row(true);

  const char *status_info_txt = ED_info_statusbar_string_ex(
      bmain, scene, view_layer, (U.statusbar_flag & ~STATUSBAR_SHOW_VERSION));
  /* True when the status is populated (delimiters required for following items). */
  bool has_status_info = false;

  if (status_info_txt[0]) {
    row->label(status_info_txt, ICON_NONE);
    has_status_info = true;
  }

  if (U.statusbar_flag & STATUSBAR_SHOW_EXTENSIONS_UPDATES) {
    wmWindowManager *wm = CTX_wm_manager(C);

    /* Special case, always show an alert for any blocked extensions. */
    if (wm->extensions_blocked > 0) {
      if (has_status_info) {
        row->separator(-0.5f);
        row->label("|", ICON_NONE);
        row->separator(-0.5f);
      }
      row->emboss_set(blender::ui::EmbossType::None);
      /* This operator also works fine for blocked extensions. */
      row->op("EXTENSIONS_OT_userpref_show_for_update", "", ICON_ERROR);
      uiBut *but = layout->block()->buttons.last().get();
      uchar color[4];
      UI_GetThemeColor4ubv(TH_TEXT, color);
      copy_v4_v4_uchar(but->col, color);

      BLI_str_format_integer_unit(but->icon_overlay_text.text, wm->extensions_blocked);
      UI_but_icon_indicator_color_set(but, color);

      row->separator(1.0f);
      has_status_info = true;
    }

    if ((G.f & G_FLAG_INTERNET_ALLOW) == 0) {
      if (has_status_info) {
        row->separator(-0.5f);
        row->label("|", ICON_NONE);
        row->separator(-0.5f);
      }

      if ((G.f & G_FLAG_INTERNET_OVERRIDE_PREF_OFFLINE) != 0) {
        row->label("", ICON_INTERNET_OFFLINE);
      }
      else {
        row->emboss_set(blender::ui::EmbossType::None);
        row->op("EXTENSIONS_OT_userpref_show_online", "", ICON_INTERNET_OFFLINE);
        uiBut *but = layout->block()->buttons.last().get();
        uchar color[4];
        UI_GetThemeColor4ubv(TH_TEXT, color);
        copy_v4_v4_uchar(but->col, color);
      }

      row->separator(1.0f);
      has_status_info = true;
    }
    else if ((wm->extensions_updates > 0) ||
             (wm->extensions_updates == WM_EXTENSIONS_UPDATE_CHECKING))
    {
      int icon = ICON_INTERNET;
      if (wm->extensions_updates == WM_EXTENSIONS_UPDATE_CHECKING) {
        icon = ICON_UV_SYNC_SELECT;
      }

      if (has_status_info) {
        row->separator(-0.5f);
        row->label("|", ICON_NONE);
        row->separator(-0.5f);
      }
      row->emboss_set(blender::ui::EmbossType::None);
      row->op("EXTENSIONS_OT_userpref_show_for_update", "", icon);
      uiBut *but = layout->block()->buttons.last().get();
      uchar color[4];
      UI_GetThemeColor4ubv(TH_TEXT, color);
      copy_v4_v4_uchar(but->col, color);

      if (wm->extensions_updates > 0) {
        BLI_str_format_integer_unit(but->icon_overlay_text.text, wm->extensions_updates);
        UI_but_icon_indicator_color_set(but, color);
      }

      row->separator(1.0f);
      has_status_info = true;
    }
  }

  if (!BKE_main_has_issues(bmain)) {
    if (U.statusbar_flag & STATUSBAR_SHOW_VERSION) {
      if (has_status_info) {
        row->separator(-0.5f);
        row->label("|", ICON_NONE);
        row->separator(-0.5f);
      }
      const char *status_info_d_txt = ED_info_statusbar_string_ex(
          bmain, scene, view_layer, STATUSBAR_SHOW_VERSION);
      row->label(status_info_d_txt, ICON_NONE);
    }
    return;
  }

  blender::StringRefNull version_string = ED_info_statusbar_string_ex(
      bmain, scene, view_layer, STATUSBAR_SHOW_VERSION);
  std::string warning_message;

  /* Blender version part is shown as warning area when there are forward compatibility issues with
   * currently loaded .blend file. */
  if (bmain->has_forward_compatibility_issues) {
    warning_message = version_string;
  }
  else {
    /* For other issues, still show the version if enabled. */
    if (U.statusbar_flag & STATUSBAR_SHOW_VERSION) {
      layout->label(version_string, ICON_NONE);
    }
  }

  /* Color space warning. */
  if (bmain->colorspace.is_missing_opencolorio_config) {
    if (!warning_message.empty()) {
      warning_message = warning_message + " ";
    }
    warning_message = warning_message + RPT_("Color Management");
  }

  const uiStyle *style = UI_style_get();
  uiLayout *ui_abs = &layout->absolute(false);
  uiBlock *block = ui_abs->block();
  blender::ui::EmbossType previous_emboss = UI_block_emboss_get(block);

  UI_fontstyle_set(&style->widget);
  const int width = max_ii(
      int(BLF_width(style->widget.uifont_id, warning_message.c_str(), warning_message.size())),
      int(10 * UI_SCALE_FAC));

  UI_block_align_begin(block);

  /* Background for icon. */
  uiBut *but = uiDefBut(block,
                        ButType::Roundbox,
                        0,
                        "",
                        0,
                        0,
                        UI_UNIT_X + (6 * UI_SCALE_FAC),
                        UI_UNIT_Y,
                        nullptr,
                        0.0f,
                        0.0f,
                        "");
  /*# ButType::Roundbox's background color is set in `but->col`. */
  UI_GetThemeColor4ubv(TH_WARNING, but->col);

  if (!warning_message.empty()) {
    /* Background for the rest of the message. */
    but = uiDefBut(block,
                   ButType::Roundbox,
                   0,
                   "",
                   UI_UNIT_X + (6 * UI_SCALE_FAC),
                   0,
                   UI_UNIT_X + width,
                   UI_UNIT_Y,
                   nullptr,
                   0.0f,
                   0.0f,
                   "");

    /* Use icon background at low opacity to highlight, but still contrasting with area TH_TEXT. */
    UI_GetThemeColor4ubv(TH_WARNING, but->col);
    but->col[3] = 64;
  }

  UI_block_align_end(block);
  UI_block_emboss_set(block, blender::ui::EmbossType::None);

  /* The warning icon itself. */
  but = uiDefIconBut(block,
                     ButType::But,
                     0,
                     ICON_ERROR,
                     int(3 * UI_SCALE_FAC),
                     0,
                     UI_UNIT_X,
                     UI_UNIT_Y,
                     nullptr,
                     0.0f,
                     0.0f,
                     std::nullopt);
  UI_but_func_tooltip_set(but, ui_template_status_tooltip, nullptr, nullptr);
  UI_GetThemeColorType4ubv(TH_INFO_WARNING_TEXT, SPACE_INFO, but->col);
  but->col[3] = 255; /* This theme color is RBG only, so have to set alpha here. */

  /* The warning message, if any. */
  if (!warning_message.empty()) {
    but = uiDefBut(block,
                   ButType::But,
                   0,
                   warning_message.c_str(),
                   UI_UNIT_X,
                   0,
                   short(width + UI_UNIT_X),
                   UI_UNIT_Y,
                   nullptr,
                   0.0f,
                   0.0f,
                   std::nullopt);
    UI_but_func_tooltip_set(but, ui_template_status_tooltip, nullptr, nullptr);
  }

  UI_block_emboss_set(block, previous_emboss);
}
