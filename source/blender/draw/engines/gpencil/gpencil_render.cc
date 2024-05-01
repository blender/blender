/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */
#include "BLI_rect.h"

#include "DRW_render.hh"

#include "BKE_object.hh"

#include "DNA_gpencil_legacy_types.h"

#include "DEG_depsgraph_query.hh"

#include "RE_pipeline.h"

#include "IMB_imbuf_types.hh"

#include "gpencil_engine.h"

void GPENCIL_render_init(GPENCIL_Data *vedata,
                         RenderEngine *engine,
                         RenderLayer *render_layer,
                         const Depsgraph *depsgraph,
                         const rcti *rect)
{
  GPENCIL_FramebufferList *fbl = vedata->fbl;
  GPENCIL_TextureList *txl = vedata->txl;

  Scene *scene = DEG_get_evaluated_scene(depsgraph);
  const float *viewport_size = DRW_viewport_size_get();
  const int size[2] = {int(viewport_size[0]), int(viewport_size[1])};

  /* Set the perspective & view matrix. */
  float winmat[4][4], viewmat[4][4], viewinv[4][4];

  Object *camera = DEG_get_evaluated_object(depsgraph, RE_GetCamera(engine->re));
  RE_GetCameraWindow(engine->re, camera, winmat);
  RE_GetCameraModelMatrix(engine->re, camera, viewinv);

  invert_m4_m4(viewmat, viewinv);

  DRWView *view = DRW_view_create(viewmat, winmat, nullptr, nullptr, nullptr);
  DRW_view_default_set(view);
  DRW_view_set_active(view);

  /* Create depth texture & color texture from render result. */
  const char *viewname = RE_GetActiveRenderView(engine->re);
  RenderPass *rpass_z_src = RE_pass_find_by_name(render_layer, RE_PASSNAME_Z, viewname);
  RenderPass *rpass_col_src = RE_pass_find_by_name(render_layer, RE_PASSNAME_COMBINED, viewname);

  float *pix_z = (rpass_z_src) ? rpass_z_src->ibuf->float_buffer.data : nullptr;
  float *pix_col = (rpass_col_src) ? rpass_col_src->ibuf->float_buffer.data : nullptr;

  if (!pix_z || !pix_col) {
    RE_engine_set_error_message(engine,
                                "Warning: To render grease pencil, enable Combined and Z passes.");
  }

  if (pix_z) {
    /* Depth need to be remapped to [0..1] range. */
    pix_z = static_cast<float *>(MEM_dupallocN(pix_z));

    int pix_num = rpass_z_src->rectx * rpass_z_src->recty;

    if (DRW_view_is_persp_get(view)) {
      for (int i = 0; i < pix_num; i++) {
        pix_z[i] = (-winmat[3][2] / -pix_z[i]) - winmat[2][2];
        pix_z[i] = clamp_f(pix_z[i] * 0.5f + 0.5f, 0.0f, 1.0f);
      }
    }
    else {
      /* Keep in mind, near and far distance are negatives. */
      float near = DRW_view_near_distance_get(view);
      float far = DRW_view_far_distance_get(view);
      float range_inv = 1.0f / fabsf(far - near);
      for (int i = 0; i < pix_num; i++) {
        pix_z[i] = (pix_z[i] + near) * range_inv;
        pix_z[i] = clamp_f(pix_z[i], 0.0f, 1.0f);
      }
    }
  }

  const bool do_region = (scene->r.mode & R_BORDER) != 0;
  const bool do_clear_z = !pix_z || do_region;
  const bool do_clear_col = !pix_col || do_region;

  /* FIXME(fclem): we have a precision loss in the depth buffer because of this re-upload.
   * Find where it comes from! */
  /* In multi view render the textures can be reused. */
  if (txl->render_depth_tx && !do_clear_z) {
    GPU_texture_update(txl->render_depth_tx, GPU_DATA_FLOAT, pix_z);
  }
  else {
    txl->render_depth_tx = DRW_texture_create_2d(
        size[0], size[1], GPU_DEPTH_COMPONENT24, DRWTextureFlag(0), do_region ? nullptr : pix_z);
  }
  if (txl->render_color_tx && !do_clear_col) {
    GPU_texture_update(txl->render_color_tx, GPU_DATA_FLOAT, pix_col);
  }
  else {
    txl->render_color_tx = DRW_texture_create_2d(
        size[0], size[1], GPU_RGBA16F, DRWTextureFlag(0), do_region ? nullptr : pix_col);
  }

  GPU_framebuffer_ensure_config(&fbl->render_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(txl->render_depth_tx),
                                    GPU_ATTACHMENT_TEXTURE(txl->render_color_tx),
                                });

  if (do_clear_z || do_clear_col) {
    /* To avoid unpredictable result, clear buffers that have not be initialized. */
    GPU_framebuffer_bind(fbl->render_fb);
    if (do_clear_col) {
      const float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      GPU_framebuffer_clear_color(fbl->render_fb, clear_col);
    }
    if (do_clear_z) {
      GPU_framebuffer_clear_depth(fbl->render_fb, 1.0f);
    }
  }

  if (do_region) {
    int x = rect->xmin;
    int y = rect->ymin;
    int w = BLI_rcti_size_x(rect);
    int h = BLI_rcti_size_y(rect);
    if (pix_col) {
      GPU_texture_update_sub(txl->render_color_tx, GPU_DATA_FLOAT, pix_col, x, y, 0, w, h, 0);
    }
    if (pix_z) {
      GPU_texture_update_sub(txl->render_depth_tx, GPU_DATA_FLOAT, pix_z, x, y, 0, w, h, 0);
    }
  }

  MEM_SAFE_FREE(pix_z);
}

/* render all objects and select only grease pencil */
static void GPENCIL_render_cache(void *vedata,
                                 Object *ob,
                                 RenderEngine * /*engine*/,
                                 Depsgraph * /*depsgraph*/)
{
  if (ob && ELEM(ob->type, OB_GPENCIL_LEGACY, OB_GREASE_PENCIL, OB_LAMP)) {
    if (DRW_object_visibility_in_active_context(ob) & OB_VISIBLE_SELF) {
      GPENCIL_cache_populate(vedata, ob);
    }
  }
}

static void GPENCIL_render_result_z(RenderLayer *rl,
                                    const char *viewname,
                                    GPENCIL_Data *vedata,
                                    const rcti *rect)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ViewLayer *view_layer = draw_ctx->view_layer;
  if ((view_layer->passflag & SCE_PASS_Z) == 0) {
    return;
  }
  RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_Z, viewname);
  if (rp == nullptr) {
    return;
  }

  float *ro_buffer_data = rp->ibuf->float_buffer.data;

  GPU_framebuffer_read_depth(vedata->fbl->render_fb,
                             rect->xmin,
                             rect->ymin,
                             BLI_rcti_size_x(rect),
                             BLI_rcti_size_y(rect),
                             GPU_DATA_FLOAT,
                             ro_buffer_data);

  float winmat[4][4];
  DRW_view_winmat_get(nullptr, winmat, false);

  int pix_num = BLI_rcti_size_x(rect) * BLI_rcti_size_y(rect);

  /* Convert GPU depth [0..1] to view Z [near..far] */
  if (DRW_view_is_persp_get(nullptr)) {
    for (int i = 0; i < pix_num; i++) {
      if (ro_buffer_data[i] == 1.0f) {
        ro_buffer_data[i] = 1e10f; /* Background */
      }
      else {
        ro_buffer_data[i] = ro_buffer_data[i] * 2.0f - 1.0f;
        ro_buffer_data[i] = winmat[3][2] / (ro_buffer_data[i] + winmat[2][2]);
      }
    }
  }
  else {
    /* Keep in mind, near and far distance are negatives. */
    float near = DRW_view_near_distance_get(nullptr);
    float far = DRW_view_far_distance_get(nullptr);
    float range = fabsf(far - near);

    for (int i = 0; i < pix_num; i++) {
      if (ro_buffer_data[i] == 1.0f) {
        ro_buffer_data[i] = 1e10f; /* Background */
      }
      else {
        ro_buffer_data[i] = ro_buffer_data[i] * range - near;
      }
    }
  }
}

static void GPENCIL_render_result_combined(RenderLayer *rl,
                                           const char *viewname,
                                           GPENCIL_Data *vedata,
                                           const rcti *rect)
{
  RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_COMBINED, viewname);
  GPENCIL_FramebufferList *fbl = ((GPENCIL_Data *)vedata)->fbl;

  GPU_framebuffer_bind(fbl->render_fb);
  GPU_framebuffer_read_color(vedata->fbl->render_fb,
                             rect->xmin,
                             rect->ymin,
                             BLI_rcti_size_x(rect),
                             BLI_rcti_size_y(rect),
                             4,
                             0,
                             GPU_DATA_FLOAT,
                             rp->ibuf->float_buffer.data);
}

void GPENCIL_render_to_image(void *ved,
                             RenderEngine *engine,
                             RenderLayer *render_layer,
                             const rcti *rect)
{
  GPENCIL_Data *vedata = (GPENCIL_Data *)ved;
  const char *viewname = RE_GetActiveRenderView(engine->re);
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Depsgraph *depsgraph = draw_ctx->depsgraph;

  GPENCIL_render_init(vedata, engine, render_layer, depsgraph, rect);
  GPENCIL_engine_init(vedata);

  vedata->stl->pd->camera = DEG_get_evaluated_object(depsgraph, RE_GetCamera(engine->re));

  /* Loop over all objects and create draw structure. */
  GPENCIL_cache_init(vedata);
  DRW_render_object_iter(vedata, engine, depsgraph, GPENCIL_render_cache);
  GPENCIL_cache_finish(vedata);

  DRW_render_instance_buffer_finish();

  /* Render the gpencil object and merge the result to the underlying render. */
  GPENCIL_draw_scene(vedata);

  GPENCIL_render_result_combined(render_layer, viewname, vedata, rect);
  GPENCIL_render_result_z(render_layer, viewname, vedata, rect);
}
