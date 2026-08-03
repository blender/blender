/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Window client-side-decorations (CSD) layout.
 */

#include "GHOST_IWindow.hh"

#include "BLI_rect.hh"

#include "WM_api.hh"
#include "wm_window.hh"
#include "wm_window_private.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Window Title Bar Layout
 *
 * Generate a client-side-decorations (CSD).
 * \{ */

int WM_window_csd_fracitonal_scale_apply(int value, const int fractional_scale[2])
{
  return (value * fractional_scale[1]) / fractional_scale[0];
}

int WM_window_csd_layout_callback(const int window_size[2],
                                  const int fractional_scale[2],
                                  const char window_state,
                                  const GHOST_CSD_Layout *csd_layout,
                                  GHOST_CSD_Elem *csd_elems)
{
  constexpr int csd_title_height = 25;

  const int title = WM_window_csd_fracitonal_scale_apply(csd_title_height, fractional_scale);

  /* The caller is expected not to run the callback for full screen windows. */
  BLI_assert(window_state != GHOST_kWindowStateFullScreen);
  UNUSED_VARS_NDEBUG(window_state);
  int decor_num = 0;

  GHOST_CSD_Elem *elem;

  /* Window contents. */
  elem = &csd_elems[decor_num++];
  elem->type = GHOST_kCSDTypeBody;
  elem->bounds[0][0] = 0;
  elem->bounds[0][1] = window_size[0];
  elem->bounds[1][0] = title;
  elem->bounds[1][1] = window_size[1];

  /* Allow this to be null for callers that only need to know about
   * the "title" & "body" regions. */
  if (csd_layout != nullptr) {
    int button_layout_title_index = 0;

    /* Buttons on the left. */
    {
      int button_index = 0;
      for (int i = 0; i < csd_layout->buttons_num; i++) {
        if (csd_layout->buttons[i] == GHOST_kCSDTypeTitlebar) {
          button_layout_title_index = i;
          break;
        }

        GHOST_TCSD_Type type = GHOST_TCSD_Type(csd_layout->buttons[i]);
        elem = &csd_elems[decor_num++];
        elem->type = type;
        elem->bounds[0][0] = title * button_index;
        elem->bounds[0][1] = title + (title * button_index);
        elem->bounds[1][0] = 0;
        elem->bounds[1][1] = title;

        button_index++;
      }
    }

    /* Buttons on the right. */
    {
      int button_index = 0;
      for (int i = csd_layout->buttons_num - 1; i > button_layout_title_index; i--) {
        GHOST_TCSD_Type type = csd_layout->buttons[i];
        elem = &csd_elems[decor_num++];
        elem->type = type;
        elem->bounds[0][0] = (window_size[0] - title) - (title * button_index);
        elem->bounds[0][1] = window_size[0] - (title * button_index);
        elem->bounds[1][0] = 0;
        elem->bounds[1][1] = title;
        button_index++;
      }
    }
  }

  /* Title bar. */
  elem = &csd_elems[decor_num++];
  elem->type = GHOST_kCSDTypeTitlebar;
  elem->bounds[0][0] = 0;
  elem->bounds[0][1] = window_size[0];
  elem->bounds[1][0] = 0;
  elem->bounds[1][1] = title;

  /* Border elements are intentionally not included, resizing runs via the invisible
   * margin outside the window which resolves borders from its own geometry,
   * see #GHOST_CSD_Params::resize_margin_size. */

  return decor_num;
}

void WM_window_csd_rect_calc(const wmWindow *win, rcti *r_rect)
{
  const GHOST_CSD_Layout *csd_layout = WM_window_csd_layout_get();
  GHOST_IWindow *ghost_window = static_cast<GHOST_IWindow *>(win->runtime->ghostwin);
  const int fractional_scale[2] = {GHOST_CSD_DPI_FRACTIONAL_BASE, ghost_window->getDPIHint()};

  GHOST_CSD_Elem csd_elems[GHOST_kCSDType_NUM];

  const int2 win_size = WM_window_native_pixel_size(win);
  const int decor_num = WM_window_csd_layout_callback(
      win_size, fractional_scale, GHOST_TWindowState(win->windowstate), csd_layout, csd_elems);

  const GHOST_CSD_Elem *elem = nullptr;
  for (int i = 0; i < decor_num; i++) {
    /* Typically the first. */
    if (csd_elems[i].type == GHOST_kCSDTypeBody) {
      elem = &csd_elems[i];
      break;
    }
  }
  if (elem == nullptr) {
    BLI_assert_msg(0, "unexpected, no window contents");
    BLI_rcti_init(r_rect, 0, win_size[0], 0, win_size[1]);
  }

  /* Flip the Y. */
  r_rect->xmin = elem->bounds[0][0];
  r_rect->xmax = elem->bounds[0][1];
  r_rect->ymin = win_size.y - elem->bounds[1][1];
  r_rect->ymax = win_size.y - elem->bounds[1][0];
}

/** \} */

}  // namespace blender
