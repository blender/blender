/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edscr
 */

#include <cstdlib>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_screen_types.h"
#include "DNA_workspace_types.h"

#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "WM_api.hh"

#include "ED_screen.hh"

#include "screen_intern.hh"

WorkSpaceLayout *ED_workspace_layout_add(Main *bmain,
                                         WorkSpace *workspace,
                                         wmWindow *win,
                                         const char *name)
{
  bScreen *screen;
  rcti screen_rect;

  WM_window_screen_rect_calc(win, &screen_rect);
  screen = screen_add(bmain, name, &screen_rect);

  return BKE_workspace_layout_add(bmain, workspace, screen, name);
}

WorkSpaceLayout *ED_workspace_layout_duplicate(Main *bmain,
                                               WorkSpace *workspace,
                                               const WorkSpaceLayout *layout_old,
                                               wmWindow *win)
{
  bScreen *screen_old = BKE_workspace_layout_screen_get(layout_old);
  const char *name = BKE_workspace_layout_name_get(layout_old);

  WorkSpaceLayout *layout_new = ED_workspace_layout_add(bmain, workspace, win, name);
  bScreen *screen_new = BKE_workspace_layout_screen_get(layout_new);

  if (BKE_screen_is_fullscreen_area(screen_old)) {
    LISTBASE_FOREACH (ScrArea *, area_old, &screen_old->areabase) {
      if (area_old->full) {
        ScrArea *area_new = (ScrArea *)screen_new->areabase.first;
        ED_area_data_copy(area_new, area_old, true);
        ED_area_tag_redraw(area_new);
        break;
      }
    }
  }
  else {
    screen_data_copy(screen_new, screen_old);
  }

  return layout_new;
}

static bool workspace_layout_delete_doit(WorkSpace *workspace,
                                         WorkSpaceLayout *layout_old,
                                         WorkSpaceLayout *layout_new,
                                         bContext *C)
{
  Main *bmain = CTX_data_main(C);
  wmWindow *win = CTX_wm_window(C);
  bScreen *screen_new = BKE_workspace_layout_screen_get(layout_new);

  ED_screen_change(C, screen_new);

  if (BKE_workspace_active_layout_get(win->workspace_hook) != layout_old) {
    BKE_workspace_layout_remove(bmain, workspace, layout_old);
    return true;
  }

  return false;
}

bool workspace_layout_set_poll(const WorkSpaceLayout *layout)
{
  const bScreen *screen = BKE_workspace_layout_screen_get(layout);

  return ((BKE_screen_is_used(screen) == false) &&
          /* in typical usage temp screens should have a nonzero winid
           * (all temp screens should be used, or closed & freed). */
          (screen->temp == false) && (BKE_screen_is_fullscreen_area(screen) == false) &&
          (screen->id.name[2] != '.' || !(U.uiflag & USER_HIDE_DOT)));
}

static WorkSpaceLayout *workspace_layout_delete_find_new(const WorkSpaceLayout *layout_old)
{
  for (WorkSpaceLayout *layout_new = layout_old->prev; layout_new; layout_new = layout_new->next) {
    if (workspace_layout_set_poll(layout_new)) {
      return layout_new;
    }
  }

  for (WorkSpaceLayout *layout_new = layout_old->next; layout_new; layout_new = layout_new->next) {
    if (workspace_layout_set_poll(layout_new)) {
      return layout_new;
    }
  }

  return nullptr;
}

bool ED_workspace_layout_delete(WorkSpace *workspace, WorkSpaceLayout *layout_old, bContext *C)
{
  const bScreen *screen_old = BKE_workspace_layout_screen_get(layout_old);
  WorkSpaceLayout *layout_new;

  BLI_assert(BLI_findindex(&workspace->layouts, layout_old) != -1);

  /* Don't allow deleting temp full-screens for now. */
  if (BKE_screen_is_fullscreen_area(screen_old)) {
    return false;
  }

  /* A layout/screen can only be in use by one window at a time, so as
   * long as we are able to find a layout/screen that is unused, we
   * can safely assume ours is not in use anywhere an delete it. */

  layout_new = workspace_layout_delete_find_new(layout_old);

  if (layout_new) {
    return workspace_layout_delete_doit(workspace, layout_old, layout_new, C);
  }

  return false;
}

static bool workspace_change_find_new_layout_cb(const WorkSpaceLayout *layout, void * /*arg*/)
{
  /* return false to stop the iterator if we've found a layout that can be activated */
  return workspace_layout_set_poll(layout) ? false : true;
}

static bScreen *screen_fullscreen_find_associated_normal_screen(const Main *bmain, bScreen *screen)
{
  LISTBASE_FOREACH (bScreen *, screen_iter, &bmain->screens) {
    if ((screen_iter != screen) && ELEM(screen_iter->state, SCREENMAXIMIZED, SCREENFULL)) {
      ScrArea *area = static_cast<ScrArea *>(screen_iter->areabase.first);
      if (area && area->full == screen) {
        return screen_iter;
      }
    }
  }

  return screen;
}

static bool screen_is_used_by_other_window(const wmWindow *win, const bScreen *screen)
{
  return BKE_screen_is_used(screen) && (screen->winid != win->winid);
}

WorkSpaceLayout *ED_workspace_screen_change_ensure_unused_layout(
    Main *bmain,
    WorkSpace *workspace,
    WorkSpaceLayout *layout_new,
    const WorkSpaceLayout *layout_fallback_base,
    wmWindow *win)
{
  WorkSpaceLayout *layout_temp = layout_new;
  bScreen *screen_temp = BKE_workspace_layout_screen_get(layout_new);

  screen_temp = screen_fullscreen_find_associated_normal_screen(bmain, screen_temp);
  layout_temp = BKE_workspace_layout_find(workspace, screen_temp);

  if (screen_is_used_by_other_window(win, screen_temp)) {
    /* Screen is already used, try to find a free one. */
    layout_temp = BKE_workspace_layout_iter_circular(
        workspace, layout_new, workspace_change_find_new_layout_cb, nullptr, false);
    screen_temp = layout_temp ? BKE_workspace_layout_screen_get(layout_temp) : nullptr;

    if (!layout_temp || screen_is_used_by_other_window(win, screen_temp)) {
      /* Fallback solution: duplicate layout. */
      layout_temp = ED_workspace_layout_duplicate(bmain, workspace, layout_fallback_base, win);
    }
  }

  return layout_temp;
}

static bool workspace_layout_cycle_iter_cb(const WorkSpaceLayout *layout, void * /*arg*/)
{
  /* return false to stop iterator when we have found a layout to activate */
  return !workspace_layout_set_poll(layout);
}

bool ED_workspace_layout_cycle(WorkSpace *workspace, const short direction, bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  WorkSpaceLayout *old_layout = BKE_workspace_active_layout_get(win->workspace_hook);
  const bScreen *old_screen = BKE_workspace_layout_screen_get(old_layout);
  ScrArea *area = CTX_wm_area(C);

  if (old_screen->temp || (area && area->full && area->full->temp)) {
    return false;
  }

  BLI_assert(ELEM(direction, 1, -1));
  WorkSpaceLayout *new_layout = BKE_workspace_layout_iter_circular(workspace,
                                                                   old_layout,
                                                                   workspace_layout_cycle_iter_cb,
                                                                   nullptr,
                                                                   (direction == -1) ? true :
                                                                                       false);

  if (new_layout && (old_layout != new_layout)) {
    bScreen *new_screen = BKE_workspace_layout_screen_get(new_layout);

    if (area && area->full) {
      /* return to previous state before switching screens */
      ED_screen_full_restore(C, area); /* may free screen of old_layout */
    }

    ED_screen_change(C, new_screen);

    return true;
  }

  return false;
}
