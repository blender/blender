/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#pragma once

struct wmWindow;
struct wmWindowManager;
struct wmXrData;

using wmXrSessionExitFn = void (*)(const wmXrData *xr_data);

/* `wm_xr.cc` */

bool wm_xr_init(wmWindowManager *wm);
void wm_xr_exit(wmWindowManager *wm);
void wm_xr_session_toggle(wmWindowManager *wm,
                          wmWindow *session_root_win,
                          wmXrSessionExitFn session_exit_fn);
bool wm_xr_events_handle(wmWindowManager *wm);

/* `wm_xr_operators.cc` */

void wm_xr_operatortypes_register();
