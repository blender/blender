/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */
#include "BLI_math_matrix.h"
#include "BLI_rect.h"

#include "DRW_render.hh"

#include "BKE_object.hh"

#include "DEG_depsgraph_query.hh"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "IMB_imbuf_types.hh"

#include "gpencil_engine.h"

void GPENCIL_render_init(GPENCIL_Data *vedata,
                         RenderEngine *engine,
                         RenderLayer *render_layer,
                         const Depsgraph *depsgraph,
                         const rcti *rect)
{
  if (vedata->instance == nullptr) {
    vedata->instance = new GPENCIL_Instance();
  }
  GPENCIL_Instance &inst = *vedata->instance;

  const float *viewport_size = DRW_viewport_size_get();
  const int size[2] = {int(viewport_size[0]), int(viewport_size[1])};

  /* Set the perspective & view matrix. */
  float winmat[4][4], viewmat[4][4], viewinv[4][4];

  Object *camera = DEG_get_evaluated_object(depsgraph, RE_GetCamera(engine->re));
  RE_GetCameraWindow(engine->re, camera, winmat);
  RE_GetCameraModelMatrix(engine->re, camera, viewinv);

  invert_m4_m4(viewmat, viewinv);

  blender::draw::View::default_set(float4x4(viewmat), float4x4(winmat));
  blender::draw::View &view = blender::draw::View::default_get();

  /* Create depth texture & color texture from render result. */
  const char *viewname = RE_GetActiveRenderView(engine->re);
  RenderPass *rpass_z_src = RE_pass_find_by_name(render_layer, RE_PASSNAME_Z, viewname);
  RenderPass *rpass_col_src = RE_pass_find_by_name(render_layer, RE_PASSNAME_COMBINED, viewname);

  float *pix_z = (rpass_z_src) ? rpass_z_src->ibuf->float_buffer.data : nullptr;
  float *pix_col = (rpass_col_src) ? rpass_col_src->ibuf->float_buffer.data : nullptr;

  if (!pix_z || !pix_col) {
    RE_engine_set_error_message(engine,
                                "Warning: To render Grease Pencil, enable Combined and Z passes.");
  }

  if (pix_z) {
    /* Depth need to be remapped to [0..1] range. */
    pix_z = static_cast<float *>(MEM_dupallocN(pix_z));

    int pix_num = rpass_z_src->rectx * rpass_z_src->recty;

    if (view.is_persp()) {
      for (int i = 0; i < pix_num; i++) {
        pix_z[i] = (-winmat[3][2] / -pix_z[i]) - winmat[2][2];
        pix_z[i] = clamp_f(pix_z[i] * 0.5f + 0.5f, 0.0f, 1.0f);
      }
    }
    else {
      /* Keep in mind, near and far distance are negatives. */
      float near = view.near_clip();
      float far = view.far_clip();
      float range_inv = 1.0f / fabsf(far - near);
      for (int i = 0; i < pix_num; i++) {
        pix_z[i] = (pix_z[i] + near) * range_inv;
        pix_z[i] = clamp_f(pix_z[i], 0.0f, 1.0f);
      }
    }
  }

  const bool do_region = !(rect->xmin == 0 && rect->ymin == 0 && rect->xmax == size[0] &&
                           rect->ymax == size[1]);
  const bool do_clear_z = !pix_z || do_region;
  const bool do_clear_col = !pix_col || do_region;

  /* FIXME(fclem): we have a precision loss in the depth buffer because of this re-upload.
   * Find where it comes from! */
  /* In multi view render the textures can be reused. */
  if (inst.render_depth_tx.is_valid() && !do_clear_z) {
    GPU_texture_update(inst.render_depth_tx, GPU_DATA_FLOAT, pix_z);
  }
  else {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT |
                             GPU_TEXTURE_USAGE_HOST_READ;
    inst.render_depth_tx.ensure_2d(
        GPU_DEPTH_COMPONENT24, int2(size), usage, do_region ? nullptr : pix_z);
  }
  if (inst.render_color_tx.is_valid() && !do_clear_col) {
    GPU_texture_update(inst.render_color_tx, GPU_DATA_FLOAT, pix_col);
  }
  else {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT |
                             GPU_TEXTURE_USAGE_HOST_READ;
    inst.render_color_tx.ensure_2d(GPU_RGBA16F, int2(size), usage, do_region ? nullptr : pix_col);
  }

  inst.render_fb.ensure(GPU_ATTACHMENT_TEXTURE(inst.render_depth_tx),
                        GPU_ATTACHMENT_TEXTURE(inst.render_color_tx));

  if (do_clear_z || do_clear_col) {
    /* To avoid unpredictable result, clear buffers that have not be initialized. */
    GPU_framebuffer_bind(inst.render_fb);
    if (do_clear_col) {
      const float clear_col[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      GPU_framebuffer_clear_color(inst.render_fb, clear_col);
    }
    if (do_clear_z) {
      GPU_framebuffer_clear_depth(inst.render_fb, 1.0f);
    }
  }

  if (do_region) {
    int x = rect->xmin;
    int y = rect->ymin;
    int w = BLI_rcti_size_x(rect);
    int h = BLI_rcti_size_y(rect);
    if (pix_col) {
      GPU_texture_update_sub(inst.render_color_tx, GPU_DATA_FLOAT, pix_col, x, y, 0, w, h, 0);
    }
    if (pix_z) {
      GPU_texture_update_sub(inst.render_depth_tx, GPU_DATA_FLOAT, pix_z, x, y, 0, w, h, 0);
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
  if (ob && ELEM(ob->type, OB_GREASE_PENCIL, OB_LAMP)) {
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

  GPU_framebuffer_read_depth(vedata->instance->render_fb,
                             rect->xmin,
                             rect->ymin,
                             BLI_rcti_size_x(rect),
                             BLI_rcti_size_y(rect),
                             GPU_DATA_FLOAT,
                             ro_buffer_data);

  float4x4 winmat = blender::draw::View::default_get().winmat();

  int pix_num = BLI_rcti_size_x(rect) * BLI_rcti_size_y(rect);

  /* Convert GPU depth [0..1] to view Z [near..far] */
  if (blender::draw::View::default_get().is_persp()) {
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
    float near = blender::draw::View::default_get().near_clip();
    float far = blender::draw::View::default_get().far_clip();
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

  GPU_framebuffer_bind(vedata->instance->render_fb);
  GPU_framebuffer_read_color(vedata->instance->render_fb,
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

  DRW_manager_get()->begin_sync();

  GPENCIL_render_init(vedata, engine, render_layer, depsgraph, rect);
  GPENCIL_engine_init(vedata);

  vedata->stl->pd->camera = DEG_get_evaluated_object(depsgraph, RE_GetCamera(engine->re));

  /* Loop over all objects and create draw structure. */
  GPENCIL_cache_init(vedata);
  DRW_render_object_iter(vedata, engine, depsgraph, GPENCIL_render_cache);
  GPENCIL_cache_finish(vedata);

  DRW_manager_get()->end_sync();

  /* Render the gpencil object and merge the result to the underlying render. */
  GPENCIL_draw_scene(vedata);

  GPENCIL_render_result_combined(render_layer, viewname, vedata, rect);
  GPENCIL_render_result_z(render_layer, viewname, vedata, rect);
}
