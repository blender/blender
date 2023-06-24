/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct rcti;

void DRW_stats_free(void);
void DRW_stats_begin(void);
void DRW_stats_reset(void);

/**
 * Use this to group the queries. It does NOT keep track
 * of the time, it only sum what the queries inside it.
 */
void DRW_stats_group_start(const char *name);
void DRW_stats_group_end(void);

/**
 * \note Only call this when no sub timer will be called.
 */
void DRW_stats_query_start(const char *name);
void DRW_stats_query_end(void);

void DRW_stats_draw(const rcti *rect);

#ifdef __cplusplus
}
#endif
