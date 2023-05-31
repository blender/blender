/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

/* wm_xr_operators.c */

void wm_xr_operatortypes_register(void);
