/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Window client-side-decorations (CSD) layout.
 */

#include "GHOST_C-api.h"

#include "WM_api.hh"
#include "wm_window.hh"
#include "wm_window_private.hh"

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
                                  GHOST_CSD_Elem *csd_elems)
{
  constexpr int csd_title_height = 25;
  constexpr int csd_border_size = 5;
  constexpr int csd_border_corner_size = csd_title_height + csd_border_size;

  const int title = WM_window_csd_fracitonal_scale_apply(csd_title_height, fractional_scale);

  /* The caller is expected not to run the callback for full screen windows. */
  BLI_assert(window_state != GHOST_kWindowStateFullScreen);
  int decor_num = 0;

  GHOST_CSD_Elem *elem;

  if (window_state == GHOST_kWindowStateMaximized) {

    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeBody;
    elem->bounds[0][0] = 0;
    elem->bounds[0][1] = window_size[0];
    elem->bounds[1][0] = title;
    elem->bounds[1][1] = window_size[1];

    /* Buttons. */
    int button_index = 0;
    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeButtonClose;
    elem->bounds[0][0] = (window_size[0] - title) - (title * button_index);
    elem->bounds[0][1] = (window_size[0]) - (title * button_index);
    elem->bounds[1][0] = 0;
    elem->bounds[1][1] = title;

    button_index++;
    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeButtonMaximize;
    elem->bounds[0][0] = (window_size[0] - title) - (title * button_index);
    elem->bounds[0][1] = (window_size[0]) - (title * button_index);
    elem->bounds[1][0] = 0;
    elem->bounds[1][1] = title;

    button_index++;
    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeButtonMinimize;
    elem->bounds[0][0] = (window_size[0] - title) - (title * button_index);
    elem->bounds[0][1] = (window_size[0]) - (title * button_index);
    elem->bounds[1][0] = 0;
    elem->bounds[1][1] = title;

    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeButtonMenu;
    elem->bounds[0][0] = 0;
    elem->bounds[0][1] = title;
    elem->bounds[1][0] = 0;
    elem->bounds[1][1] = title;

    /* Title bar. */
    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeTitlebar;
    elem->bounds[0][0] = 0;
    elem->bounds[0][1] = window_size[0];
    elem->bounds[1][0] = 0;
    elem->bounds[1][1] = title;
  }
  else { /* #GHOST_kWindowStateNormal */
    const int32_t border = WM_window_csd_fracitonal_scale_apply(csd_border_size, fractional_scale);
    const int32_t border_corner = WM_window_csd_fracitonal_scale_apply(csd_border_corner_size,
                                                                       fractional_scale);
    /* Window contents. */
    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeBody;
    elem->bounds[0][0] = border;
    elem->bounds[0][1] = window_size[0] - border;
    elem->bounds[1][0] = border + title;
    elem->bounds[1][1] = window_size[1] - border;

    /* Buttons. */
    int button_index = 0;
    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeButtonClose;
    elem->bounds[0][0] = (window_size[0] - (border + title)) - (title * button_index);
    elem->bounds[0][1] = (window_size[0] - (border)) - (title * button_index);
    elem->bounds[1][0] = border;
    elem->bounds[1][1] = border + title;

    button_index++;
    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeButtonMaximize;
    elem->bounds[0][0] = (window_size[0] - (border + title)) - (title * button_index);
    elem->bounds[0][1] = (window_size[0] - (border)) - (title * button_index);
    elem->bounds[1][0] = border;
    elem->bounds[1][1] = border + title;

    button_index++;
    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeButtonMinimize;
    elem->bounds[0][0] = (window_size[0] - (border + title)) - (title * button_index);
    elem->bounds[0][1] = (window_size[0] - (border)) - (title * button_index);
    elem->bounds[1][0] = border;
    elem->bounds[1][1] = border + title;

    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeButtonMenu;
    elem->bounds[0][0] = border;
    elem->bounds[0][1] = border + title;
    elem->bounds[1][0] = border;
    elem->bounds[1][1] = border + title;

    /* Title bar. */
    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeTitlebar;
    elem->bounds[0][0] = border;
    elem->bounds[0][1] = window_size[0] - border;
    elem->bounds[1][0] = border;
    elem->bounds[1][1] = border + title;

    /* Border: corners. */
    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeBorderTopLeft;
    elem->bounds[0][0] = 0;
    elem->bounds[0][1] = border_corner;
    elem->bounds[1][0] = 0;
    elem->bounds[1][1] = border_corner;

    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeBorderTopRight;
    elem->bounds[0][0] = window_size[0] - border_corner;
    elem->bounds[0][1] = window_size[0];
    elem->bounds[1][0] = 0;
    elem->bounds[1][1] = border_corner;

    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeBorderBottomLeft;
    elem->bounds[0][0] = 0;
    elem->bounds[0][1] = border_corner;
    elem->bounds[1][0] = window_size[1] - border_corner;
    elem->bounds[1][1] = window_size[1];

    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeBorderBottomRight;
    elem->bounds[0][0] = window_size[0] - border_corner;
    elem->bounds[0][1] = window_size[0];
    elem->bounds[1][0] = window_size[1] - border_corner;
    elem->bounds[1][1] = window_size[1];

    /* Border: axis aligned. */
    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeBorderTop;
    elem->bounds[0][0] = 0;
    elem->bounds[0][1] = window_size[0];
    elem->bounds[1][0] = 0;
    elem->bounds[1][1] = border;

    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeBorderBottom;
    elem->bounds[0][0] = 0;
    elem->bounds[0][1] = window_size[0];
    elem->bounds[1][0] = window_size[1] - border;
    elem->bounds[1][1] = window_size[1];

    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeBorderLeft;
    elem->bounds[0][0] = 0;
    elem->bounds[0][1] = border;
    elem->bounds[1][0] = 0;
    elem->bounds[1][1] = window_size[1];

    elem = &csd_elems[decor_num++];
    elem->type = GHOST_kCSDTypeBorderRight;
    elem->bounds[0][0] = window_size[0] - border;
    elem->bounds[0][1] = window_size[0];
    elem->bounds[1][0] = 0;
    elem->bounds[1][1] = window_size[1];
  }

  return decor_num;
}

void WM_window_csd_rect_calc(const wmWindow *win, rcti *r_rect)
{
  const int fractional_scale[2] = {
      GHOST_CSD_DPI_FRACTIONAL_BASE,
      GHOST_GetDPIHint(static_cast<GHOST_WindowHandle>(win->runtime->ghostwin)),
  };

  GHOST_CSD_Elem csd_elems[GHOST_kCSDType_NUM];

  const blender::int2 win_size = WM_window_native_pixel_size(win);
  const int decor_num = WM_window_csd_layout_callback(
      win_size, fractional_scale, GHOST_TWindowState(win->windowstate), csd_elems);

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
