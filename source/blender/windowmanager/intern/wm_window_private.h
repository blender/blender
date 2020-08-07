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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

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
