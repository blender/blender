/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup wm
 */
#pragma once

#include "BLI_sys_types.h"
#include "GHOST_Types.h"

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
