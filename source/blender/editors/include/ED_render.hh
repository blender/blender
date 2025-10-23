/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "DNA_ID_enums.h"
#include "DNA_material_types.h"
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
struct uiPreview;
struct ViewLayer;
struct World;
struct wmWindow;
struct wmWindowManager;

/* `render_ops.cc` */

void ED_operatortypes_render();

/* `render_update.cc` */

void ED_render_engine_changed(Main *bmain, bool update_scene_data);
void ED_render_engine_area_exit(Main *bmain, ScrArea *area);
void ED_render_view_layer_changed(Main *bmain, bScreen *screen);

/* Callbacks handling data update events coming from depsgraph. */

void ED_render_id_flush_update(const DEGEditorUpdateContext *update_ctx, ID *id);
/**
 * Update all 3D viewport render and draw engines on changes to the scene.
 * This is called by the dependency graph when it detects changes.
 */
void ED_render_scene_update(const DEGEditorUpdateContext *update_ctx, bool updated);
/**
 * Update 3D viewport render or draw engine on changes to the scene or view settings.
 */
void ED_render_view3d_update(Depsgraph *depsgraph, wmWindow *window, ScrArea *area, bool updated);

Scene *ED_render_job_get_scene(const bContext *C);
Scene *ED_render_job_get_current_scene(const bContext *C);

/**
 * Render the preview method.
 */
enum ePreviewRenderMethod {
  /** Preview is rendered for buttons window. */
  PR_BUTS_RENDER = 0,
  /** Preview is rendered for icons. hopefully fast enough for at least 32x32. */
  PR_ICON_RENDER = 1,
  /** No render, we just ensure deferred icon data gets generated. */
  PR_ICON_DEFERRED = 2,
};

bool ED_check_engine_supports_preview(const Scene *scene);
const char *ED_preview_collection_name(ePreviewType pr_type);

void ED_preview_ensure_dbase(bool with_gpencil);
void ED_preview_free_dbase();

/**
 * For preview icons loaded from disk (deferred loading), use the size of the source image, and
 * only scale to the display size when drawing. Then we actually know the final display size
 * (so we don't scale twice), and can scale on the GPU while drawing.
 */
bool ED_preview_use_image_size(const PreviewImage *preview, eIconSizes size);
/**
 * Check if \a id is supported by the automatic preview render.
 */
bool ED_preview_id_is_supported(const ID *id, const char **r_disabled_hint = nullptr);

void ED_preview_set_visibility(Main *pr_main,
                               Scene *scene,
                               ViewLayer *view_layer,
                               ePreviewType pr_type,
                               ePreviewRenderMethod pr_method);
World *ED_preview_prepare_world(Main *pr_main,
                                const Scene *scene,
                                const World *world,
                                ID_Type id_type,
                                ePreviewRenderMethod pr_method);
World *ED_preview_prepare_world_simple(Main *bmain);
void ED_preview_world_simple_set_rgb(World *world, const float color[4]);

void ED_preview_shader_job(const bContext *C,
                           const void *owner,
                           ID *id,
                           ID *parent,
                           MTex *slot,
                           int sizex,
                           int sizey,
                           ePreviewRenderMethod method);
void ED_preview_icon_render(
    const bContext *C, Scene *scene, PreviewImage *prv_img, ID *id, enum eIconSizes icon_size);
void ED_preview_icon_job(
    const bContext *C, PreviewImage *prv_img, ID *id, enum eIconSizes icon_size, bool delay);

void ED_preview_restart_queue_free();
void ED_preview_restart_queue_add(ID *id, enum eIconSizes size);
void ED_preview_restart_queue_work(const bContext *C);

void ED_preview_kill_jobs(wmWindowManager *wm, Main *bmain);
void ED_preview_kill_jobs_for_id(wmWindowManager *wm, const ID *id);

void ED_preview_draw(
    const bContext *C, void *idp, void *parentp, void *slotp, uiPreview *ui_preview, rcti *rect);

/**
 * For UI previews (i.e. #uiPreview, not #PreviewImage): Tag all previews for \a id as dirty, so
 * the next redraw triggers a re-render in #ED_preview_draw().
 */
void ED_previews_tag_dirty_by_id(const Main &bmain, const ID &id);

void ED_render_clear_mtex_copybuf();

void ED_render_internal_init();
