/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */
#pragma once

#include "GHOST_Types.h"

#include "GPU_platform_backend_enum.h"

struct GHOST_CSD_Layout;
struct bContext;
struct wmWindow;

/* *************** Message box *************** */
/* `WM_ghost_show_message_box` is implemented in `wm_windows.c` it is
 * defined here as it was implemented to be used for showing
 * a message to the user when the platform is not (fully) supported.
 *
 * In all other cases this message box should not be used. */
void WM_ghost_show_message_box(const char *title,
                               const char *message,
                               const char *help_label,
                               const char *continue_label,
                               const char *link,
                               GHOST_DialogOptions dialog_options);

GHOST_TDrawingContextType wm_ghost_drawing_context_type(const GPUBackendType gpu_backend);

void wm_test_gpu_backend_fallback(bContext *C);

/* wm_window_csd_draw.cc */

/**
 * \param win_size: The window size from GHOST, un-scaled.
 * \param win_state: The window state (normal, maximized etc).
 * \param is_active: The active state of the window.
 * \param dpi: The DPI returned by GHOST (no UI scale preferences).
 * \param title: The window title or null to display no title.
 * \param font_id: The font to display the title.
 * \param font_size: The font size to display the  title
 * \param border_color: The border color or null of the CSD to display as an overlay
 * (used by the animation player).
 * \param alpha: Transparency so decorations can be an overlay that is "faded" out
 * (used by the animation player).
 */
void WM_window_csd_draw_titlebar_ex(const int win_size[2],
                                    char win_state,
                                    const GHOST_CSD_Layout *csd_layout,
                                    bool is_active,
                                    const uint16_t dpi,
                                    const char *title,
                                    int font_id,
                                    int font_size,
                                    const uchar border_color[3],
                                    const uchar text_color[3],
                                    float alpha);
void WM_window_csd_draw_titlebar(const wmWindow *win);

/* wm_window_csd_layout.cc */

/**
 * Apply fractional scale for client side decorations.
 */
int WM_window_csd_fracitonal_scale_apply(int value, const int fractional_scale[2]);
/**
 * Callback for GHOST that defines the layout of client side decorations.
 *
 * Also used to calculate the visible area of a window when #WM_window_is_csd returns true.
 *
 * \param csd_layout: When null, buttons won't be included.
 */
int WM_window_csd_layout_callback(const int window_size[2],
                                  const int fractional_scale[2],
                                  char window_state,
                                  const GHOST_CSD_Layout *csd_layout,
                                  GHOST_CSD_Elem *csd_elems);

const GHOST_CSD_Layout *WM_window_csd_layout_get();
