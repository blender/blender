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
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_RENDER_H__
#define __ED_RENDER_H__

#include "DNA_vec_types.h"

struct DEGEditorUpdateContext;
struct ID;
struct MTex;
struct Main;
struct Render;
struct Scene;
struct ScrArea;
struct bContext;
struct bScreen;
struct wmWindowManager;

/* render_ops.c */

void ED_operatortypes_render(void);

/* render_update.c */

void ED_render_engine_changed(struct Main *bmain);
void ED_render_engine_area_exit(struct Main *bmain, struct ScrArea *sa);
void ED_render_view_layer_changed(struct Main *bmain, struct bScreen *sc);

/* Callbacks handling data update events coming from depsgraph. */

void ED_render_id_flush_update(const struct DEGEditorUpdateContext *update_ctx, struct ID *id);
void ED_render_scene_update(const struct DEGEditorUpdateContext *update_ctx, int updated);

struct Scene *ED_render_job_get_scene(const struct bContext *C);
struct Scene *ED_render_job_get_current_scene(const struct bContext *C);

/* Render the preview
 *
 * pr_method:
 * - PR_BUTS_RENDER: preview is rendered for buttons window
 * - PR_ICON_RENDER: preview is rendered for icons. hopefully fast enough for at least 32x32
 * - PR_NODE_RENDER: preview is rendered for node editor
 * - PR_ICON_DEFERRED: No render, we just ensure deferred icon data gets generated.
 */

enum {
  PR_BUTS_RENDER = 0,
  PR_ICON_RENDER = 1,
  PR_NODE_RENDER = 2,
  PR_ICON_DEFERRED = 3,
};

void ED_preview_ensure_dbase(void);
void ED_preview_free_dbase(void);

void ED_preview_shader_job(const struct bContext *C,
                           void *owner,
                           struct ID *id,
                           struct ID *parent,
                           struct MTex *slot,
                           int sizex,
                           int sizey,
                           int method);
void ED_preview_icon_render(struct Main *bmain,
                            struct Scene *scene,
                            struct ID *id,
                            unsigned int *rect,
                            int sizex,
                            int sizey);
void ED_preview_icon_job(const struct bContext *C,
                         void *owner,
                         struct ID *id,
                         unsigned int *rect,
                         int sizex,
                         int sizey,
                         const bool delay);
void ED_preview_kill_jobs(struct wmWindowManager *wm, struct Main *bmain);

void ED_preview_draw(const struct bContext *C, void *idp, void *parentp, void *slot, rcti *rect);

void ED_render_clear_mtex_copybuf(void);

void ED_render_internal_init(void);

#endif
