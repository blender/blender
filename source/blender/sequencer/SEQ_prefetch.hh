/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct bContext;
struct Scene;

void SEQ_prefetch_stop_all();
/**
 * Use also to update scene and context changes
 * This function should almost always be called by cache invalidation, not directly.
 */
void SEQ_prefetch_stop(Scene *scene);
bool SEQ_prefetch_need_redraw(const bContext *C, Scene *scene);
