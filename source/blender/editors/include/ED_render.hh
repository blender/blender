/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "DNA_ID_enums.h"
#include "DNA_vec_types.h"

struct DEGEditorUpdateContext;
struct Depsgraph;
struct ID;
struct MTex;
struct Main;
struct Render;
struct Scene;
struct ScrArea;
struct bContext;
struct bScreen;
struct PreviewImage;
struct wmWindow;
struct wmWindowManager;

/* `render_ops.cc` */

void ED_operatortypes_render(void);

/* `render_update.cc` */

void ED_render_engine_changed(struct Main *bmain, bool update_scene_data);
void ED_render_engine_area_exit(struct Main *bmain, struct ScrArea *area);
void ED_render_view_layer_changed(struct Main *bmain, struct bScreen *screen);

/* Callbacks handling data update events coming from depsgraph. */

void ED_render_id_flush_update(const struct DEGEditorUpdateContext *update_ctx, struct ID *id);
/**
 * Update all 3D viewport render and draw engines on changes to the scene.
 * This is called by the dependency graph when it detects changes.
 */
void ED_render_scene_update(const struct DEGEditorUpdateContext *update_ctx, bool updated);
/**
 * Update 3D viewport render or draw engine on changes to the scene or view settings.
 */
void ED_render_view3d_update(struct Depsgraph *depsgraph,
                             struct wmWindow *window,
                             struct ScrArea *area,
                             bool updated);

struct Scene *ED_render_job_get_scene(const struct bContext *C);
struct Scene *ED_render_job_get_current_scene(const struct bContext *C);

/* Render the preview
 *
 * pr_method:
 * - PR_BUTS_RENDER: preview is rendered for buttons window
 * - PR_ICON_RENDER: preview is rendered for icons. hopefully fast enough for at least 32x32
 * - PR_ICON_DEFERRED: No render, we just ensure deferred icon data gets generated.
 */
typedef enum ePreviewRenderMethod {
  PR_BUTS_RENDER = 0,
  PR_ICON_RENDER = 1,
  PR_ICON_DEFERRED = 2,
} ePreviewRenderMethod;

void ED_preview_ensure_dbase(void);
void ED_preview_free_dbase(void);

/**
 * Check if \a id is supported by the automatic preview render.
 */
bool ED_preview_id_is_supported(const struct ID *id);

void ED_preview_shader_job(const struct bContext *C,
                           void *owner,
                           struct ID *id,
                           struct ID *parent,
                           struct MTex *slot,
                           int sizex,
                           int sizey,
                           ePreviewRenderMethod method);
void ED_preview_icon_render(const struct bContext *C,
                            struct Scene *scene,
                            struct PreviewImage *prv_img,
                            struct ID *id,
                            enum eIconSizes icon_size);
void ED_preview_icon_job(const struct bContext *C,
                         struct PreviewImage *prv_img,
                         struct ID *id,
                         enum eIconSizes icon_size,
                         bool delay);

void ED_preview_restart_queue_free(void);
void ED_preview_restart_queue_add(struct ID *id, enum eIconSizes size);
void ED_preview_restart_queue_work(const struct bContext *C);

void ED_preview_kill_jobs(struct wmWindowManager *wm, struct Main *bmain);

void ED_preview_draw(const struct bContext *C, void *idp, void *parentp, void *slot, rcti *rect);

void ED_render_clear_mtex_copybuf(void);

void ED_render_internal_init(void);
