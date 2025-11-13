/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \name WM-Surface
 *
 * Container to manage painting in an off-screen context.
 */

#pragma once

#include "GHOST_Types.h"

struct bContext;
struct GPUContext;

struct wmSurface {
  wmSurface *next, *prev;

  GHOST_ContextHandle system_gpu_context;
  GPUContext *blender_gpu_context;

  void *customdata;

  void (*draw)(bContext *);
  /** To evaluate the surface's depsgraph. Called as part of the main loop. */
  void (*do_depsgraph)(bContext *C);
  /** Free customdata, not the surface itself (done by wm_surface API). */
  void (*free_data)(wmSurface *);

  /** Called when surface is activated for drawing (made drawable). */
  void (*activate)();
  /** Called when surface is deactivated for drawing (current drawable cleared). */
  void (*deactivate)();
};

/* Create/Free. */

void wm_surface_add(wmSurface *surface);
void wm_surface_remove(wmSurface *surface);
void wm_surfaces_free();

/* Utils. */

void wm_surfaces_iter(bContext *C, void (*cb)(bContext *, wmSurface *));

/* Evaluation. */

void wm_surfaces_do_depsgraph(bContext *C);

/* Drawing. */

void wm_surface_make_drawable(wmSurface *surface);
void wm_surface_clear_drawable();
void wm_surface_set_drawable(wmSurface *surface, bool activate);
void wm_surface_reset_drawable();
