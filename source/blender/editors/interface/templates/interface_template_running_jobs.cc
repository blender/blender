/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <fmt/format.h>

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_main.hh"

#include "BLI_string.h"
#include "BLI_time.h"

#include "BLI_timecode.h"
#include "BLT_translation.hh"

#include "ED_screen.hh"

#include "WM_api.hh"

#include "UI_interface.hh"
#include "interface_intern.hh"

#define B_STOPRENDER 1
#define B_STOPCAST 2
#define B_STOPANIM 3
#define B_STOPCOMPO 4
#define B_STOPSEQ 5
#define B_STOPCLIP 6
#define B_STOPFILE 7
#define B_STOPOTHER 8

static void do_running_jobs(bContext *C, void * /*arg*/, int event)
{
  switch (event) {
    case B_STOPRENDER:
      G.is_break = true;
      break;
    case B_STOPCAST:
      WM_jobs_stop_all_from_owner(CTX_wm_manager(C), CTX_wm_screen(C));
      break;
    case B_STOPANIM:
      WM_operator_name_call(C, "SCREEN_OT_animation_play", WM_OP_INVOKE_SCREEN, nullptr, nullptr);
      break;
    case B_STOPCOMPO:
      WM_jobs_stop_all_from_owner(CTX_wm_manager(C), CTX_data_scene(C));
      break;
    case B_STOPSEQ:
      WM_jobs_stop_all_from_owner(CTX_wm_manager(C), CTX_data_scene(C));
      break;
    case B_STOPCLIP:
      WM_jobs_stop_all_from_owner(CTX_wm_manager(C), CTX_data_scene(C));
      break;
    case B_STOPFILE:
      WM_jobs_stop_all_from_owner(CTX_wm_manager(C), CTX_data_scene(C));
      break;
    case B_STOPOTHER:
      G.is_break = true;
      break;
  }
}

struct ProgressTooltip_Store {
  wmWindowManager *wm;
  void *owner;
};

static std::string progress_tooltip_func(bContext * /*C*/, void *argN, const char * /*tip*/)
{
  ProgressTooltip_Store *arg = static_cast<ProgressTooltip_Store *>(argN);
  wmWindowManager *wm = arg->wm;
  void *owner = arg->owner;

  const float progress = WM_jobs_progress(wm, owner);

  /* create tooltip text and associate it with the job */
  char elapsed_str[32];
  char remaining_str[32] = "Unknown";
  const double elapsed = BLI_time_now_seconds() - WM_jobs_starttime(wm, owner);
  BLI_timecode_string_from_time_simple(elapsed_str, sizeof(elapsed_str), elapsed);

  if (progress) {
    const double remaining = (elapsed / double(progress)) - elapsed;
    BLI_timecode_string_from_time_simple(remaining_str, sizeof(remaining_str), remaining);
  }

  return fmt::format(
      "Time Remaining: {}\n"
      "Time Elapsed: {}",
      remaining_str,
      elapsed_str);
}

void uiTemplateRunningJobs(uiLayout *layout, bContext *C)
{
  Main *bmain = CTX_data_main(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  ScrArea *area = CTX_wm_area(C);
  void *owner = nullptr;
  int handle_event, icon = 0;
  const char *op_name = nullptr;
  const char *op_description = nullptr;

  uiBlock *block = uiLayoutGetBlock(layout);
  UI_block_layout_set_current(block, layout);

  UI_block_func_handle_set(block, do_running_jobs, nullptr);

  /* another scene can be rendering too, for example via compositor */
  LISTBASE_FOREACH (Scene *, scene, &bmain->scenes) {
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_ANY)) {
      handle_event = B_STOPOTHER;
      icon = ICON_NONE;
      owner = scene;
    }
    else {
      continue;
    }

    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_SEQ_BUILD_PROXY)) {
      handle_event = B_STOPSEQ;
      icon = ICON_SEQUENCE;
      owner = scene;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_SEQ_BUILD_PREVIEW)) {
      handle_event = B_STOPSEQ;
      icon = ICON_SEQUENCE;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_SEQ_DRAW_THUMBNAIL)) {
      handle_event = B_STOPSEQ;
      icon = ICON_SEQUENCE;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_CLIP_BUILD_PROXY)) {
      handle_event = B_STOPCLIP;
      icon = ICON_TRACKER;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_CLIP_PREFETCH)) {
      handle_event = B_STOPCLIP;
      icon = ICON_TRACKER;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_CLIP_TRACK_MARKERS)) {
      handle_event = B_STOPCLIP;
      icon = ICON_TRACKER;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_CLIP_SOLVE_CAMERA)) {
      handle_event = B_STOPCLIP;
      icon = ICON_TRACKER;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_FILESEL_READDIR) ||
        WM_jobs_test(wm, scene, WM_JOB_TYPE_ASSET_LIBRARY_LOAD))
    {
      handle_event = B_STOPFILE;
      icon = ICON_FILEBROWSER;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_RENDER)) {
      handle_event = B_STOPRENDER;
      icon = ICON_SCENE;
      if (U.render_display_type != USER_RENDER_DISPLAY_NONE) {
        op_name = "RENDER_OT_view_show";
        op_description = "Show the render window";
      }
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_COMPOSITE)) {
      handle_event = B_STOPCOMPO;
      icon = ICON_RENDERLAYERS;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_OBJECT_BAKE_TEXTURE) ||
        WM_jobs_test(wm, scene, WM_JOB_TYPE_OBJECT_BAKE))
    {
      /* Skip bake jobs in compositor to avoid compo header displaying
       * progress bar which is not being updated (bake jobs only need
       * to update NC_IMAGE context.
       */
      if (area->spacetype != SPACE_NODE) {
        handle_event = B_STOPOTHER;
        icon = ICON_IMAGE;
        break;
      }
      continue;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_DPAINT_BAKE)) {
      handle_event = B_STOPOTHER;
      icon = ICON_MOD_DYNAMICPAINT;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_POINTCACHE)) {
      handle_event = B_STOPOTHER;
      icon = ICON_PHYSICS;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_OBJECT_SIM_FLUID)) {
      handle_event = B_STOPOTHER;
      icon = ICON_MOD_FLUIDSIM;
      break;
    }
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_OBJECT_SIM_OCEAN)) {
      handle_event = B_STOPOTHER;
      icon = ICON_MOD_OCEAN;
      break;
    }
  }

  if (owner) {
    const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
    const bool active = !(G.is_break || WM_jobs_is_stopped(wm, owner));

    uiLayout *row = uiLayoutRow(layout, false);
    block = uiLayoutGetBlock(row);

    /* get percentage done and set it as the UI text */
    const float progress = WM_jobs_progress(wm, owner);
    char text[8];
    SNPRINTF(text, "%d%%", int(progress * 100));

    const char *name = active ? WM_jobs_name(wm, owner) : "Canceling...";

    /* job icon as a button */
    if (op_name) {
      uiDefIconButO(block,
                    UI_BTYPE_BUT,
                    op_name,
                    WM_OP_INVOKE_DEFAULT,
                    icon,
                    0,
                    0,
                    UI_UNIT_X,
                    UI_UNIT_Y,
                    TIP_(op_description));
    }

    /* job name and icon if not previously set */
    const int textwidth = UI_fontstyle_string_width(fstyle, name);
    uiDefIconTextBut(block,
                     UI_BTYPE_LABEL,
                     0,
                     op_name ? 0 : icon,
                     name,
                     0,
                     0,
                     textwidth + UI_UNIT_X * 1.5f,
                     UI_UNIT_Y,
                     nullptr,
                     0.0f,
                     0.0f,
                     "");

    /* stick progress bar and cancel button together */
    row = uiLayoutRow(layout, true);
    uiLayoutSetActive(row, active);
    block = uiLayoutGetBlock(row);

    {
      ProgressTooltip_Store *tip_arg = static_cast<ProgressTooltip_Store *>(
          MEM_mallocN(sizeof(*tip_arg), __func__));
      tip_arg->wm = wm;
      tip_arg->owner = owner;
      uiButProgress *but_progress = (uiButProgress *)uiDefIconTextBut(block,
                                                                      UI_BTYPE_PROGRESS,
                                                                      0,
                                                                      ICON_NONE,
                                                                      text,
                                                                      UI_UNIT_X,
                                                                      0,
                                                                      UI_UNIT_X * 6.0f,
                                                                      UI_UNIT_Y,
                                                                      nullptr,
                                                                      0.0f,
                                                                      0.0f,
                                                                      nullptr);

      but_progress->progress_factor = progress;
      UI_but_func_tooltip_set(but_progress, progress_tooltip_func, tip_arg, MEM_freeN);
    }

    if (!wm->runtime->is_interface_locked) {
      uiDefIconTextBut(block,
                       UI_BTYPE_BUT,
                       handle_event,
                       ICON_PANEL_CLOSE,
                       "",
                       0,
                       0,
                       UI_UNIT_X,
                       UI_UNIT_Y,
                       nullptr,
                       0.0f,
                       0.0f,
                       TIP_("Stop this job"));
    }
  }

  if (ED_screen_animation_no_scrub(wm)) {
    uiDefIconTextBut(block,
                     UI_BTYPE_BUT,
                     B_STOPANIM,
                     ICON_CANCEL,
                     IFACE_("Anim Player"),
                     0,
                     0,
                     UI_UNIT_X * 5.0f,
                     UI_UNIT_Y,
                     nullptr,
                     0.0f,
                     0.0f,
                     TIP_("Stop animation playback"));
  }
}
