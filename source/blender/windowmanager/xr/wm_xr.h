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
 * \ingroup wm
 */

#pragma once

struct wmWindowManager;
struct wmXrData;

typedef void (*wmXrSessionExitFn)(const wmXrData *xr_data);

/* wm_xr.c */
bool wm_xr_init(wmWindowManager *wm);
void wm_xr_exit(wmWindowManager *wm);
void wm_xr_session_toggle(wmWindowManager *wm, wmWindow *win, wmXrSessionExitFn session_exit_fn);
bool wm_xr_events_handle(wmWindowManager *wm);
