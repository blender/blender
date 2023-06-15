/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct wmWindowManager;

/* info_stats.c */

void ED_info_stats_clear(struct wmWindowManager *wm, struct ViewLayer *view_layer);
const char *ED_info_statusbar_string(struct Main *bmain,
                                     struct Scene *scene,
                                     struct ViewLayer *view_layer);

const char *ED_info_statistics_string(struct Main *bmain,
                                      struct Scene *scene,
                                      struct ViewLayer *view_layer);

/**
 * \param v3d_local: Pass this argument to calculate view-port local statistics.
 * Note that this must only be used for local-view, otherwise report specific statistics
 * will be written into the global scene statistics giving incorrect results.
 */
void ED_info_draw_stats(struct Main *bmain,
                        struct Scene *scene,
                        struct ViewLayer *view_layer,
                        struct View3D *v3d_local,
                        int x,
                        int *y,
                        int height);

#ifdef __cplusplus
}
#endif
