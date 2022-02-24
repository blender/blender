/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct bContext;

#ifdef __cplusplus
extern "C" {
#endif

bool CURVES_SCULPT_mode_poll(struct bContext *C);
bool CURVES_SCULPT_mode_poll_view3d(struct bContext *C);

#ifdef __cplusplus
}
#endif
