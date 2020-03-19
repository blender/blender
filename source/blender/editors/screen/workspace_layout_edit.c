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
 */

/** \file
 * \ingroup edscr
 */

#include <stdlib.h>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_screen_types.h"
#include "DNA_workspace_types.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

#include "WM_api.h"

#include "ED_screen.h"

#include "screen_intern.h"

/**
 * Empty screen, with 1 dummy area without spacedata. Uses window size.
 */
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
  bScreen *screen_new;
  WorkSpaceLayout *layout_new;

  layout_new = ED_workspace_layout_add(bmain, workspace, win, name);
  screen_new = BKE_workspace_layout_screen_get(layout_new);

  if (BKE_screen_is_fullscreen_area(screen_old)) {
    for (ScrArea *area_old = screen_old->areabase.first; area_old; area_old = area_old->next) {
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

  return NULL;
}

/**
 * \warning Only call outside of area/region loops!
 * \return true if succeeded.
 */
bool ED_workspace_layout_delete(WorkSpace *workspace, WorkSpaceLayout *layout_old, bContext *C)
{
  const bScreen *screen_old = BKE_workspace_layout_screen_get(layout_old);
  WorkSpaceLayout *layout_new;

  BLI_assert(BLI_findindex(BKE_workspace_layouts_get(workspace), layout_old) != -1);

  /* don't allow deleting temp fullscreens for now */
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

static bool workspace_layout_cycle_iter_cb(const WorkSpaceLayout *layout, void *UNUSED(arg))
{
  /* return false to stop iterator when we have found a layout to activate */
  return !workspace_layout_set_poll(layout);
}

bool ED_workspace_layout_cycle(WorkSpace *workspace, const short direction, bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  WorkSpaceLayout *old_layout = BKE_workspace_active_layout_get(win->workspace_hook);
  WorkSpaceLayout *new_layout;
  const bScreen *old_screen = BKE_workspace_layout_screen_get(old_layout);
  ScrArea *sa = CTX_wm_area(C);

  if (old_screen->temp || (sa && sa->full && sa->full->temp)) {
    return false;
  }

  BLI_assert(ELEM(direction, 1, -1));
  new_layout = BKE_workspace_layout_iter_circular(workspace,
                                                  old_layout,
                                                  workspace_layout_cycle_iter_cb,
                                                  NULL,
                                                  (direction == -1) ? true : false);

  if (new_layout && (old_layout != new_layout)) {
    bScreen *new_screen = BKE_workspace_layout_screen_get(new_layout);

    if (sa && sa->full) {
      /* return to previous state before switching screens */
      ED_screen_full_restore(C, sa); /* may free screen of old_layout */
    }

    ED_screen_change(C, new_screen);

    return true;
  }

  return false;
}
