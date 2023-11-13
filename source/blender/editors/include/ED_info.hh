/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct Main;
struct wmWindowManager;

/* `info_stats.cc` */

void ED_info_stats_clear(wmWindowManager *wm, ViewLayer *view_layer);
const char *ED_info_statusbar_string_ex(Main *bmain,
                                        Scene *scene,
                                        ViewLayer *view_layer,
                                        const char statusbar_flag);
const char *ED_info_statusbar_string(Main *bmain, Scene *scene, ViewLayer *view_layer);

const char *ED_info_statistics_string(Main *bmain, Scene *scene, ViewLayer *view_layer);

/**
 * \param v3d_local: Pass this argument to calculate view-port local statistics.
 * Note that this must only be used for local-view, otherwise report specific statistics
 * will be written into the global scene statistics giving incorrect results.
 */
void ED_info_draw_stats(Main *bmain,
                        Scene *scene,
                        ViewLayer *view_layer,
                        View3D *v3d_local,
                        int x,
                        int *y,
                        int height);
