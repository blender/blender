/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_wayland_dynload
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

bool wayland_dynload_client_init(bool verbose);
void wayland_dynload_client_exit(void);

bool wayland_dynload_cursor_init(bool verbose);
void wayland_dynload_cursor_exit(void);

bool wayland_dynload_egl_init(bool verbose);
void wayland_dynload_egl_exit(void);

#ifdef WITH_GHOST_WAYLAND_LIBDECOR
bool wayland_dynload_libdecor_init(bool verbose);
void wayland_dynload_libdecor_exit(void);
#endif

#ifdef __cplusplus
}
#endif
